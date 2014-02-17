#include <stdio.h>
#include "system.h"
#include "includes.h"
#include "altera_avalon_pio_regs.h"
#include "sys/alt_irq.h"
#include "sys/alt_alarm.h"
#include <sys/alt_timestamp.h>
 
 
#define DEBUG 0
#define DEBUG_IO 1
#define DEBUG_OLD 1
 
#define HW_TIMER_PERIOD 100 /* 100ms */
 
/* Button Patterns */
 
#define GAS_PEDAL_FLAG      0x08
#define BRAKE_PEDAL_FLAG    0x04
#define CRUISE_CONTROL_FLAG 0x02
/* Switch Patterns */
 
#define TOP_GEAR_FLAG       0x00000002
#define ENGINE_FLAG         0x00000001
 
/* LED Patterns */
 
#define LED_RED_0 0x00000001 // Engine
#define LED_RED_1 0x00000002 // Top Gear
 
#define LED_GREEN_0 0x0001 // Cruise Control activated
#define LED_GREEN_2 0x0002 // Cruise Control Button
#define LED_GREEN_4 0x0010 // Brake Pedal
#define LED_GREEN_6 0x0040 // Gas Pedal
 
/*
 * Definition of Tasks
 */
 
#define StartTask_STACKSIZE 1800
#define ControlTask_STACKSIZE 600
#define VehicleTask_STACKSIZE 1800
#define ButtonIO_STACKSIZE 600
#define SwitchIO_STACKSIZE 600
#define WatchDog_STACKSIZE 1800
#define OLD_STACKSIZE 600
#define DYNAMIC_UTI_STACKSIZE 1800
 
OS_STK StartTask_Stack[StartTask_STACKSIZE];
OS_STK ControlTask_Stack[ControlTask_STACKSIZE];
OS_STK VehicleTask_Stack[VehicleTask_STACKSIZE];
OS_STK ButtonIO_Stack[ButtonIO_STACKSIZE];
OS_STK SwitchIO_Stack[SwitchIO_STACKSIZE];
OS_STK WatchDog_Stack[WatchDog_STACKSIZE];
OS_STK OLD_Stack[OLD_STACKSIZE];
OS_STK DYNAMIC_UTI_Stack[DYNAMIC_UTI_STACKSIZE];
 
// Task Priorities
 
 
#define OLD_PRIO 16
#define DYNAMIC_UTI_PRIO 14
#define CONTROLTASK_PRIO  12
#define VEHICLETASK_PRIO  10
#define SwitchIO_PRIO 8
#define ButtonIO_PRIO  7
#define WatchDog_PRIO 6
#define STARTTASK_PRIO 5
 
 
 
// Task Periods
 
#define CONTROL_PERIOD  300
#define VEHICLE_PERIOD  300
#define ButtonIO_PERIOD  300
#define SwitchIO_PERIOD  300
#define WatchDog_PERIOD  300
#define OLD_PERIOD  400
#define DYNAMIC_UTI_PERIOD 300
 
#define Threshold_Velocity_CC 200 // 20.0m/s
 
/*
 * Definition of Kernel Objects
*/
 
// Mailboxes
OS_EVENT *Mbox_Throttle;
OS_EVENT *Mbox_Velocity;
 
// Semaphores
 
// SW-Timer
OS_TMR *SWTimer_VehicleTask, *SWTimer_ControlTask, *SWTimer_ButtonIO, *SWTimer_SwitchIO, *SWTimer_DYNAMIC_UTI ;
OS_EVENT *Sem_VehicleTask, *Sem_ControlTask, *Sem_ButtonIO, *Sem_SwitchIO, *Sem_ButtonIO_IP, *Sem_SwitchIO_IP, *Sem_DYNAMIC_UTI, *Sem_WDT_Signal;
OS_STK_DATA stk_data;
 
 
void Tmr_Callback_Control_Task () {
    INT8U err;
 
    err = OSSemPost(Sem_ControlTask);
    switch (err) {
        case OS_ERR_NONE:
            if(DEBUG)
            printf("Released SEM_CONTROL_TASK\n");
            break;
           
        case OS_ERR_SEM_OVF:
            printf("Error Sem_VehicleTask POST Control Task \n");
            break;
    }
}
 
void Tmr_Callback_Vehicle_Task () {
    INT8U err;
 
    err = OSSemPost(Sem_VehicleTask);
    switch (err) {
        case OS_ERR_NONE:
            if(DEBUG)
            printf("Released SEM_VEHICLE_TASK\n");
            break;
           
        case OS_ERR_SEM_OVF:
            printf("Error Sem_VehicleTask POST Vehicle Task \n");
            break;
    }
}
 
void Tmr_Callback_SwitchIO_Task () {
    INT8U err;
 
    err = OSSemPost(Sem_SwitchIO);
    switch (err) {
        case OS_ERR_NONE:
            if(DEBUG)
            printf("Released SEM_SWITCH_IO_TASK \n");
            break;
           
        case OS_ERR_SEM_OVF:
            printf("Error SEM_Switch_IO POST Task \n");
            break;
    }
}
 
void Tmr_Callback_ButtonIO_Task () {
    INT8U err;
 
    err = OSSemPost(Sem_ButtonIO);
    switch (err) {
        case OS_ERR_NONE:
            if(DEBUG)
            printf("Released SEM_BUTTON_IO_TASK\n");
            break;
           
        case OS_ERR_SEM_OVF:
            printf("Error SEM_Button IO POST \n");
            break;
    }
}
 
void Tmr_Callback_DYNAMIC_UTI_Task () {
    INT8U err;
 
    err = OSSemPost(Sem_DYNAMIC_UTI);
    switch (err) {
        case OS_ERR_NONE:
            if(DEBUG)
            printf("Released SEM_DYNAMIC_UTI_TASK\n");
            break;
           
        case OS_ERR_SEM_OVF:
            printf("Error Sem_DYNAMIC_UTI POST Task \n");
            break;
    }
}
/*
 * Types
 */
enum active {on, off};
 
enum active gas_pedal = off;
enum active brake_pedal = off;
enum active top_gear = off;
enum active engine = off;
enum active cruise_control = off;
 
/*
 * Global variables
 */
int delay; // Delay of HW-timer
int REF_VELOCITY = 0, switchIO_out;
INT16S CUR_VELOCITY = 0; /* Value between -200 and 700 (-20.0 m/s amd 70.0 m/s) */
 
int buttons_pressed(void)
{
  return ~IORD_ALTERA_AVALON_PIO_DATA(DE2_PIO_KEYS4_BASE);    
}
 
int switches_pressed(void)
{
  return IORD_ALTERA_AVALON_PIO_DATA(DE2_PIO_TOGGLES18_BASE);    
}
 
/*
 * ISR for HW Timer
 */
alt_u32 alarm_handler(void* context)
{
  OSTmrSignal(); /* Signals a 'tick' to the SW timers */
 
  return delay;
}
 
static int b2sLUT[] = {0x40, //0
                 0x79, //1
                 0x24, //2
                 0x30, //3
                 0x19, //4
                 0x12, //5
                 0x02, //6
                 0x78, //7
                 0x00, //8
                 0x18, //9
                 0x3F, //-
};
 
/*
 * convert int to seven segment display format
 */
int int2seven(int inval){
    return b2sLUT[inval];
}
 
/*
 * output current velocity on the seven segement display
 */
void show_velocity_on_sevenseg(INT8S velocity){
  int tmp = velocity;
  int out;
  INT8U out_high = 0;
  INT8U out_low = 0;
  INT8U out_sign = 0;
 
  if(velocity < 0){
     out_sign = int2seven(10);
     tmp *= -1;
  }else{
     out_sign = int2seven(0);
  }
 
  out_high = int2seven(tmp / 10);
  out_low = int2seven(tmp - (tmp/10) * 10);
 
  out = int2seven(0) << 21 |
            out_sign << 14 |
            out_high << 7  |
            out_low;
  IOWR_ALTERA_AVALON_PIO_DATA(DE2_PIO_HEX_LOW28_BASE,out);
}
 
/*
 * shows the target velocity on the seven segment display (HEX5, HEX4)
 * when the cruise control is activated (0 otherwise)
 */
void show_target_velocity(INT8U target_val)
{
  int out, diff;
  INT8U out_high = 0, out_high_diff = 0;
  INT8U out_low = 0, out_low_diff = 0;
 
  diff = (CUR_VELOCITY/10) - target_val;
  diff = (diff<0) ? (-1 * diff) : diff ;
 
  out_high = int2seven(target_val / 10);
  out_low = int2seven(target_val - (target_val/10) * 10);
 
  out_high_diff = int2seven(diff / 10);
  out_low_diff = int2seven(diff - (diff/10) * 10);
 
  out = out_high_diff << 21 |  out_low_diff << 14 | out_high << 7  | out_low;
 
  IOWR_ALTERA_AVALON_PIO_DATA(DE2_PIO_HEX_HIGH28_BASE,out);
}
 
/*
 * indicates the position of the vehicle on the track with the four leftmost red LEDs
 * LEDR17: [0m, 400m)
 * LEDR16: [400m, 800m)
 * LEDR15: [800m, 1200m)
 * LEDR14: [1200m, 1600m)
 * LEDR13: [1600m, 2000m)
 * LEDR12: [2000m, 2400m]
 */
void show_position(INT16U position)
{
    int range = position / 4000, led_out = 0;
       
    switch(range){
        case 0:                                 // 0m - 400m
            led_out = led_out | 131072;       // Enable 17th bit
            break;
        case 1:                             // 400m - 800m
            led_out = led_out | 196608;    //  Enable 17, 16th bit
            break;
        case 2:                             // 800m - 1200m
            led_out = led_out | 229376;     //  Enable 17, 16, 15th bit
            break;
        case 3:                                // 1200m - 1600m
            led_out = led_out | 245760;         //  Enable 17, 16, 15, 14th bit
            break;
        case 4:                                // 1600m - 2000m
            led_out = led_out | 253952;       //  Enable 17, 16, 15, 14, 13th bit
            break;
        case 5:                           // 2000m - 2400m
            led_out = led_out | 258048;  //Enable 17, 16, 15, 14, 13, 12th bit
            break;
        default:
            printf("----- Error in Show_Position LED display -----");
            break;
    }
    led_out = led_out + switchIO_out;                          
    IOWR_ALTERA_AVALON_PIO_DATA(DE2_PIO_REDLED18_BASE,led_out);
}
 
/*
* Show Status of Switches and Buttons in LED  
*/
void show_active_signals () {
    if(DEBUG)
    printf("Showing Active signals in LED \n");
   
    INT8U buttIO_out = 0;
   
    buttIO_out = (gas_pedal == on) ? (buttIO_out | 64) : (buttIO_out & 63); // Gas_Pedal LED6 Control
     
    buttIO_out = (brake_pedal == on) ? (buttIO_out | 16) : (buttIO_out & 111); // Brake_Pedal LED4
     
    buttIO_out = (cruise_control == on) ? (buttIO_out | 4) : (buttIO_out & 123); // Cruise_Control LED2
   
        IOWR_ALTERA_AVALON_PIO_DATA(DE2_PIO_GREENLED9_BASE,buttIO_out);
 
    switchIO_out = (top_gear == on) ? (switchIO_out | 2) : (switchIO_out & 1); // Top_Gear
   
    switchIO_out = (engine == on) ? (switchIO_out | 1) : (switchIO_out & 2); // Engine
       
}
 
 
/*
 * The function 'adjust_position()' adjusts the position depending on the
 * acceleration and velocity.
 */
 INT16U adjust_position(INT16U position, INT16S velocity,
                        INT8S acceleration, INT16U time_interval)
{
  INT16S new_position = position + velocity * time_interval / 1000
    + acceleration / 2  * (time_interval / 1000) * (time_interval / 1000);
 
  if (new_position > 24000) {
    new_position -= 24000;
  } else if (new_position < 0){
    new_position += 24000;
  }
 
  show_position(new_position);
  return new_position;
}
 
/*
 * The function 'adjust_velocity()' adjusts the velocity depending on the
 * acceleration.
 */
INT16S adjust_velocity(INT16S velocity, INT8S acceleration,  
               enum active brake_pedal, INT16U time_interval)
{
  INT16S new_velocity;
  INT8U brake_retardation = 200;
 
  if (brake_pedal == off)
    new_velocity = velocity  + (float) (acceleration * time_interval) / 1000.0;
  else {
    if (brake_retardation * time_interval / 1000 > velocity)
      new_velocity = 0;
    else
      new_velocity = velocity - brake_retardation * time_interval / 1000;
  }
 
  return new_velocity;
}
 
/*
 * The task 'VehicleTask' updates the current velocity of the vehicle
 */
void VehicleTask(void* pdata)
{
  INT8U err, err_tmr_1, err_tmr_start, err_sem;  
  void* msg;
  INT8U* throttle = 0;
  INT8S acceleration;  /* Value between 40 and -20 (4.0 m/s^2 and -2.0 m/s^2) */
  INT8S retardation;   /* Value between 20 and -10 (2.0 m/s^2 and -1.0 m/s^2) */
  INT16U position = 0; /* Value between 0 and 20000 (0.0 m and 2000.0 m)  */
  INT16S wind_factor;   /* Value between -10 and 20 (2.0 m/s^2 and -1.0 m/s^2) */
 
  Sem_VehicleTask = OSSemCreate(0);
 
  printf("Vehicle task created!\n");
   
 
    SWTimer_VehicleTask = OSTmrCreate(0, VEHICLE_PERIOD, OS_TMR_OPT_PERIODIC, Tmr_Callback_Vehicle_Task, (void *)0,"S/W Timer 2",&err_tmr_1);
    if(err_tmr_1 == OS_ERR_NONE){
        if(DEBUG)
        printf("Timer for VECHICLE TASK is created\n");
       
        OSTmrStart(SWTimer_VehicleTask, &err_tmr_start);
       
        if(err_tmr_start == OS_ERR_NONE){
            if(DEBUG)
            printf("Timer started in VEHICLE TASK \n");
        }
        else {
            printf("Problem starting timer in VEHICLE TASK \n");
        }      
      }
      else {
      printf("Error while creating Vehicle task timer \n");    
     
      if(err_tmr_1 == OS_ERR_TMR_INVALID_DLY)
      printf(" Delay INVALID : VEHICLE TASK\n");
     
      }
 
  while(1)
    {
        // Wait for user inputs
        OSSemPend(Sem_SwitchIO_IP , 0, &err_sem);
        OSSemPend(Sem_ButtonIO_IP, 0, &err_sem);
       
        if((brake_pedal == on) && (CUR_VELOCITY > 0)){
            CUR_VELOCITY = CUR_VELOCITY - 10;
           
            if(DEBUG_IO)
            printf("********** Brake brake_pedal : %d ********** \n", CUR_VELOCITY);
        }
       
      err = OSMboxPost(Mbox_Velocity, (void *) &CUR_VELOCITY);
     
      if(DEBUG)
      printf("Vehicle task Posted MBoxPost_Velocity \n");
 
        if(DEBUG)
        printf("SEM ACCESS VEHICLE TASK\n\n");
       
        OSSemPend(Sem_VehicleTask,0,&err_sem);
       
      /* Non-blocking read of mailbox:
       - message in mailbox: update throttle
       - no message:         use old throttle
      */
      if(DEBUG)
      printf("Vehicle task waiting for MBoxPost_Throttle for 1 unit time \n");
     
      msg = OSMboxPend(Mbox_Throttle, 1, &err);
      if (err == OS_NO_ERR)
         throttle = (INT8U*) msg;
 
      if(DEBUG)
      printf("Vehicle task GOT MBoxPost_Throttle \n");
     
      /* Retardation : Factor of Terrain and Wind Resistance */
      if (CUR_VELOCITY > 0)
         wind_factor = CUR_VELOCITY * CUR_VELOCITY / 10000 + 1;
      else
         wind_factor = (-1) * CUR_VELOCITY * CUR_VELOCITY / 10000 + 1;
         
      if (position < 4000)
         retardation = wind_factor; // even ground
      else if (position < 8000)
          retardation = wind_factor + 15; // traveling uphill
        else if (position < 12000)
            retardation = wind_factor + 25; // traveling steep uphill
          else if (position < 16000)
              retardation = wind_factor; // even ground
            else if (position < 20000)
                retardation = wind_factor - 10; //traveling downhill
              else
                  retardation = wind_factor - 5 ; // traveling steep downhill
                 
      acceleration = *throttle / 2 - retardation;    
      position = adjust_position(position, CUR_VELOCITY, acceleration, 300);
      CUR_VELOCITY = adjust_velocity(CUR_VELOCITY, acceleration, brake_pedal, 300);
      printf("Position: %dm\n", position / 10);
      printf("Velocity: %4.1fm/s\n", CUR_VELOCITY /10.0);
      printf("Throttle: %dV Time : %d \n\n", *throttle / 10, (int) (alt_timestamp() / (alt_timestamp_freq()/1000)));
      alt_timestamp_start();
  /*    
  INT32U stk_size;
  err = OSTaskStkChk(16, &stk_data);
      stk_size = stk_data.OSUsed;
      printf("Stack Used OverLoadDetection : %d \n", stk_size);
     
      err = OSTaskStkChk(14, &stk_data);
      stk_size = stk_data.OSUsed;
      printf("Stack Used DYNAMIV_UTI : %d \n", stk_size);
     
      err = OSTaskStkChk(12, &stk_data);
      stk_size = stk_data.OSUsed;
      printf("Stack Used CONTROLTASK: %d \n", stk_size);
     
      err = OSTaskStkChk(10, &stk_data);
      stk_size = stk_data.OSUsed;
      printf("Stack Used VehicleTask: %d \n", stk_size);
     
      err = OSTaskStkChk(8, &stk_data);
      stk_size = stk_data.OSUsed;
      printf("Stack Used SwitchIO: %d \n", stk_size);
     
      err = OSTaskStkChk(7, &stk_data);
      stk_size = stk_data.OSUsed;
      printf("Stack Used ButtonIO: %d \n", stk_size);
     
      err = OSTaskStkChk(6, &stk_data);
      stk_size = stk_data.OSUsed;
      printf("Stack Used WatchDog: %d \n", stk_size);
     
      err = OSTaskStkChk(5, &stk_data);
      stk_size = stk_data.OSUsed;
      printf("Stack Used Start Task: %d \n", stk_size);
*/
      show_velocity_on_sevenseg((INT8S) (CUR_VELOCITY / 10));
      show_active_signals();
      show_position(position);
     
    }
}
 
/*
 * The task 'ControlTask' is the main task of the application. It reacts
 * on sensors and generates responses.
 */
 
void ControlTask(void* pdata)
{
  INT8U err, err_tmr_2, err_tmr_start, err_sem;
  INT8U throttle = 0; /* Value between 0 and 80, which is interpreted as between 0.0V and 8.0V */
  void* msg;
  INT16S* current_velocity, diff_velocity;
  Sem_ControlTask = OSSemCreate(0);
  printf("Control Task created!\n");
 
  SWTimer_ControlTask = OSTmrCreate(0, CONTROL_PERIOD, OS_TMR_OPT_PERIODIC , Tmr_Callback_Control_Task, (void *)0,"S/W Timer 1",&err_tmr_2);  
   
    if(err_tmr_2 == OS_ERR_NONE){
        if(DEBUG)
        printf("Timer for CONTROL TASK is created and Started\n");
       
        OSTmrStart(SWTimer_ControlTask, &err_tmr_start);    
         
      }
      else {
      printf("Error while creating control task timer \n");    
     
      if(err_tmr_2 == OS_ERR_TMR_NON_AVAIL)
      printf(" OS_ERR_TMR_NON_AVAIL : CONTROL TASK \n");
     
      }
 
  while(1)
    {
      if(DEBUG)
      printf("Control task waiting for MBoxPost_Velocity \n\n");
     
      msg = OSMboxPend(Mbox_Velocity, 0, &err);
      current_velocity = (INT16S*) msg;
     
      // Set throttle value if gas pedal is pressed
      if((gas_pedal == on) && (engine == on)){
        throttle = throttle + 10;
       
        if(throttle > 80)
        throttle = 80;  // Max value of throttle 0 - 80
       
        if(DEBUG_IO)
        printf("********** Acclerate Gas_Pedal : %d **********\n", throttle);
      }
      else {
        throttle = 0;
      }
     
      if(engine == off){
        throttle = 0;
      }
     
     
      // Check for cruise control
      if((cruise_control == on) && (engine == on)){
       
        if(REF_VELOCITY == 0)
        REF_VELOCITY = *current_velocity;
       
        if(DEBUG_IO)
        printf("********** Cruise control Active ********** REF : %d \n",REF_VELOCITY);
       
        show_target_velocity((INT8U) (REF_VELOCITY/10));
       
       
        if(*current_velocity > REF_VELOCITY){    // Decrease throttle value
             diff_velocity = *current_velocity - REF_VELOCITY;
             
            if(diff_velocity > 15){
                throttle = 0;
            }
            else if(diff_velocity > 10){
                throttle = throttle - 30;
            }
            else {
                throttle = throttle - 10;
            }
           
            if(throttle <= 0) // Throttle  0-80
            throttle = 0;
           
            if(DEBUG_IO)
            printf("DEC throttle : %d \n", throttle);
        }
        else {
            diff_velocity = REF_VELOCITY - *current_velocity;
           
            if(diff_velocity > 15){
                throttle = 80;
            }
            else if (diff_velocity > 10){
                throttle = throttle + 30;
            }
            else {
               throttle = throttle + 10;
            }
           
            if(throttle >= 80)
            throttle = 80;
           
            if(DEBUG_IO)
            printf("INC throttle : %d \n", throttle);
        }
      }
      else {
        //cruise_control = off;
        REF_VELOCITY = 0;
        IOWR_ALTERA_AVALON_PIO_DATA(DE2_PIO_HEX_HIGH28_BASE,0x0000);
      }
     
      err = OSMboxPost(Mbox_Throttle, (void *) &throttle);
     
     
      if(DEBUG)
      printf("MBoxPost_Throttle done by CONTROL TASK \n");
 
        if(DEBUG)
        printf("SEM ACCESS CONTROL TASK\n\n");
       
        OSSemPend(Sem_ControlTask, 0, &err_sem);
         
        if(DEBUG)
        printf("Control Task in Loop\n");
         
    }
}
 
 
void ButtonIO(void* pdata)
{
  INT8U  err_tmr, err_tmr_start, err_sem;
  int button_status;
  Sem_ButtonIO = OSSemCreate(0);
  Sem_ButtonIO_IP = OSSemCreate(0); // For getting user Input
 
  printf("ButtonIO Task created!\n");
 
  SWTimer_ButtonIO = OSTmrCreate(0, ButtonIO_PERIOD, OS_TMR_OPT_PERIODIC , Tmr_Callback_ButtonIO_Task, (void *)0,"ButtonIO",&err_tmr);  
   
    if(err_tmr == OS_ERR_NONE){
        if(DEBUG)
        printf("Timer for ButtonIO is created\n");
       
         OSTmrStart(SWTimer_ButtonIO, &err_tmr_start);  
       
       if(err_tmr_start == OS_ERR_NONE){        
           if(DEBUG)
           printf("Timer Started in ButtonIO \n");
        }
        else {
            printf("Problem starting timer in ButtonIO TASK \n");
        }        
      }
      else {
      printf("Error while creating ButtonIO task timer \n");    
     
      if(err_tmr == OS_ERR_TMR_INVALID_OPT)
      printf(" OS_ERR_TMR_INVALID_OPT : ButtonIO TASK \n");
     
      }
 
  while(1)
    {
      if(DEBUG)
      printf("ButtonIO task \n");
     
      button_status = buttons_pressed();
     
      if(DEBUG)
      printf("Button Status : %d, %x \n",button_status, button_status & 15);
     
      button_status = button_status & 15;
     
      if(button_status & 4) // Brake Pedal pressed
      {
       
        brake_pedal = on;
        cruise_control = off;      
      }
      else {
            brake_pedal = off;
        }
     
      if((button_status & 1) && (engine == on)) // Gas Pedal pressed
      {
        gas_pedal = on;
        cruise_control = off;
      }
      else {
          gas_pedal = off;
      }  
     
      // Cruse Contol pressed
      if((button_status & 2) && (top_gear == on) && (CUR_VELOCITY >= Threshold_Velocity_CC) && (gas_pedal == off) && (brake_pedal == off))
      {
        if(DEBUG_IO)
        printf("Cruise control detected -----");
       
        cruise_control = on;
      }  
 
      OSSemPost(Sem_ButtonIO_IP);  
       
        if(DEBUG)
        printf("SEM ACCESS ButtonIO TASK\n\n");
           
        OSSemPend(Sem_ButtonIO, 0, &err_sem);
       
        if(DEBUG)
        printf("ButtonIO Task in Loop\n");        
    }
}
 
 
void SwitchIO(void* pdata)
{
  INT8U err_tmr, err_tmr_start, err_sem, switch_status;
 
  Sem_SwitchIO = OSSemCreate(0);
  Sem_SwitchIO_IP = OSSemCreate(0); // For getting user input
 
  printf("SwitchIO Task created!\n");
 
  SWTimer_SwitchIO = OSTmrCreate(0, SwitchIO_PERIOD, OS_TMR_OPT_PERIODIC , Tmr_Callback_SwitchIO_Task, (void *)0,"SwitchIO",&err_tmr);  
   
    if(err_tmr == OS_ERR_NONE){
        OSTmrStart(SWTimer_SwitchIO, &err_tmr_start);
       
        if(DEBUG)
        printf("Timer for SwitchIO is created and Started \n");        
      }
      else {
      printf("Error while creating SwitchIO task timer \n");    
     
      if(err_tmr == OS_ERR_TMR_NAME_TOO_LONG)
      printf(" OS_ERR_TMR_NAME_TOO_LONG : SwitchIO TASK \n");
         
      }
 
  while(1)
    {
      if(DEBUG)
      printf("SwitchIO task \n");
     
       switch_status = switches_pressed();
       
       if(DEBUG)
       printf("Switch Status : %d \n", switch_status);
       
       if(switch_status & 1) // Engine pressed
       {
            engine = on;
       }
       else if(CUR_VELOCITY == 0){
            engine = off;
       }
       
       if(switch_status & 2) // Top Gear pressed
       {
            top_gear = on;
       }
       else {
            top_gear = off;
            cruise_control = off;
       }
       
       OSSemPost(Sem_SwitchIO_IP);
       
        if(DEBUG)
        printf("SEM ACCESS SwitchIO TASK\n\n");
       
        OSSemPend(Sem_SwitchIO, 0, &err_sem);        
         
        if(DEBUG)
        printf("SwitchIO Task in Loop\n");      
    }
}
 
void WatchDog(void* pdata)
{
  INT8U err;
 
  printf("WatchDog Task created! : Ticks : %d\n",(int) alt_timestamp_freq() );
 
  Sem_WDT_Signal = OSSemCreate(0);
 
  while(1)
    {
        OSSemPend(Sem_WDT_Signal, WatchDog_PERIOD, &err);
       
        if(err == OS_ERR_NONE){
            //if(DEBUG_OLD)
            //printf(" OK \n");
        }
        else {
            //printf("Pend Error\n");
           
            if(err == OS_ERR_TIMEOUT)
           printf("------ Watchdog Timer ALERT : Over Load Detected ------- : %d \n",(int) (alt_timestamp() / (alt_timestamp_freq()/1000)));
           
           if(err == OS_ERR_PEND_LOCKED)
           printf("SEM got LOCKED \n\n");
        }
    }
}
 
void OLD(void* pdata)
{
  INT8U err;
 
  printf("OLD Task created!\n");
 
  while(1)
    {
         err = OSSemPost(Sem_WDT_Signal);
         
        if(err == OS_ERR_NONE){
            //if(DEBUG_IO)
            //printf("----- Running Low Priority Task : OK ------ \n");
        }
        else {
            printf("Error Posting value \n");
           
            if(err == OS_ERR_SEM_OVF)
            printf("SEM FULL \n");
        }
    }
}
 
 
void DYNAMIC_UTI(void* pdata)
{
  INT8U err_tmr, err_tmr_start, i, err;
  int j, switch_status;
 
  Sem_DYNAMIC_UTI = OSSemCreate(0);
 
  printf("DYNAMIC_UTI Task created!\n");
 
  SWTimer_DYNAMIC_UTI = OSTmrCreate(0, DYNAMIC_UTI_PERIOD, OS_TMR_OPT_PERIODIC , Tmr_Callback_DYNAMIC_UTI_Task, (void *)0,"DYNAMIC_UTI",&err_tmr);  
   
    if(err_tmr == OS_ERR_NONE){
        OSTmrStart(SWTimer_DYNAMIC_UTI, &err_tmr_start);
       
        if(DEBUG)
        printf("Timer for DYNAMIC_UTI is created and Started \n");        
      }
      else {
      printf("Error while creating DYNAMIC_UTI task timer \n");    
      }
 
  while(1)
    {
     
       switch_status = switches_pressed();
       switch_status = switch_status & 1008; // Mask for SW4 - SW9
       
       switch_status = switch_status >> 4;
       printf("Before loop : Time : %d, SwitchStatus : %d \n", (int) (alt_timestamp() / (alt_timestamp_freq()/1000)), switch_status);
        //printf("switch_status : %d\n",switch_status);
       
       if(switch_status > 50)
       switch_status = 50;
       
       // Loop running for 6 unit of time
       // for Optimization Level Os : j = 6000000 ;
       // for No Optimization : j = 3444
       for(i=0; i<switch_status; i++){
        for(j=0; j<6000000; j++){
        }
       }
       printf("After loop : Time : %d \n", (int) (alt_timestamp() / (alt_timestamp_freq()/1000)));
       OSSemPend(Sem_DYNAMIC_UTI, 0, &err);
       
       if(err == OS_ERR_NONE)
       {
       }
       else {
        printf("Sem_DYNAMIC_UTI Pend Error ------ \n");
       }
    }
}
/*
 * The task 'StartTask' creates all other tasks kernel objects and
 * deletes itself afterwards.
 */
 
void StartTask(void* pdata)
{
  INT8U err;
  void* context;
   
 
  static alt_alarm alarm;     /* Is needed for timer ISR function */
 
  /* Base resolution for SW timer : HW_TIMER_PERIOD ms */
  //delay = alt_ticks_per_second() * HW_TIMER_PERIOD / 1000;
 // printf("delay in ticks %d\n", delay);
 
  /*
   * Create Hardware Timer with a period of 'delay'
   */
  if (alt_alarm_start (&alarm,
      delay,
      alarm_handler,
      context) < 0)
      {
          printf("No system clock available!n");
      }
 
  /*
   * Create and start Software Timer
   */
 
  /*
   * Creation of Kernel Objects
   */
 
  // Mailboxes
  Mbox_Throttle = OSMboxCreate((void*) 0); /* Empty Mailbox - Throttle */
  Mbox_Velocity = OSMboxCreate((void*) 0); /* Empty Mailbox - Velocity */
   
  /*
   * Create statistics task
   */
 
  OSStatInit();
 
  /*
   * Creating Tasks in the system
   */
 
 
  err = OSTaskCreateExt(
       ControlTask, // Pointer to task code
       NULL,        // Pointer to argument that is
                    // passed to task
       &ControlTask_Stack[ControlTask_STACKSIZE-1], // Pointer to top
                             // of task stack
       CONTROLTASK_PRIO,
       CONTROLTASK_PRIO,
       (void *)&ControlTask_Stack[0],
       ControlTask_STACKSIZE,
       (void *) 0,
       OS_TASK_OPT_STK_CHK);
 
  err = OSTaskCreateExt(
       VehicleTask, // Pointer to task code
       NULL,        // Pointer to argument that is
                    // passed to task
       &VehicleTask_Stack[VehicleTask_STACKSIZE-1], // Pointer to top
                             // of task stack
       VEHICLETASK_PRIO,
       VEHICLETASK_PRIO,
       (void *)&VehicleTask_Stack[0],
       VehicleTask_STACKSIZE,
       (void *) 0,
       OS_TASK_OPT_STK_CHK);
       
    err = OSTaskCreateExt(
       ButtonIO, // Pointer to task code
       NULL,        // Pointer to argument that is
                    // passed to task
       &ButtonIO_Stack[ButtonIO_STACKSIZE-1], // Pointer to top
                             // of task stack
       ButtonIO_PRIO,
       ButtonIO_PRIO,
       (void *)&ButtonIO_Stack[0],
       ButtonIO_STACKSIZE,
       (void *) 0,
       OS_TASK_OPT_STK_CHK);
 
   err = OSTaskCreateExt(
        SwitchIO, // Pointer to task code
       NULL,        // Pointer to argument that is
                    // passed to task
       &SwitchIO_Stack[SwitchIO_STACKSIZE-1], // Pointer to top
                             // of task stack
       SwitchIO_PRIO,
       SwitchIO_PRIO,
       (void *)&SwitchIO_Stack[0],
       SwitchIO_STACKSIZE,
       (void *) 0,
       OS_TASK_OPT_STK_CHK);
       
   err = OSTaskCreateExt(
        WatchDog, // Pointer to task code
       NULL,        // Pointer to argument that is
                    // passed to task
       &WatchDog_Stack[WatchDog_STACKSIZE-1], // Pointer to top
                             // of task stack
       WatchDog_PRIO,
       WatchDog_PRIO,
       (void *)&WatchDog_Stack[0],
       WatchDog_STACKSIZE,
       (void *) 0,
       OS_TASK_OPT_STK_CHK);
       
    err = OSTaskCreateExt(
        OLD, // Pointer to task code
       NULL,        // Pointer to argument that is
                    // passed to task
       &OLD_Stack[OLD_STACKSIZE-1], // Pointer to top
                             // of task stack
       OLD_PRIO,
       OLD_PRIO,
       (void *)&OLD_Stack[0],
       OLD_STACKSIZE,
       (void *) 0,
       OS_TASK_OPT_STK_CHK);
     
     err = OSTaskCreateExt(
        DYNAMIC_UTI, // Pointer to task code
       NULL,        // Pointer to argument that is
                    // passed to task
       &DYNAMIC_UTI_Stack[DYNAMIC_UTI_STACKSIZE-1], // Pointer to top
                             // of task stack
       DYNAMIC_UTI_PRIO,
       DYNAMIC_UTI_PRIO,
       (void *)&DYNAMIC_UTI_Stack[0],
       DYNAMIC_UTI_STACKSIZE,
       (void *) 0,
       OS_TASK_OPT_STK_CHK);
       
  printf("All Tasks and Kernel Objects generated!\n");
 
 
  /* Task deletes itself */
 
  OSTaskDel(OS_PRIO_SELF);
}
 
/*
 *
 * The function 'main' creates only a single task 'StartTask' and starts
 * the OS. All other tasks are started from the task 'StartTask'.
 *
 */
 
int main(void) {
 
  printf("Lab: Cruise Control\n");
 alt_timestamp_start();
  OSTaskCreateExt(
     StartTask, // Pointer to task code
         NULL,      // Pointer to argument that is
                    // passed to task
         (void *)&StartTask_Stack[StartTask_STACKSIZE-1], // Pointer to top
                             // of task stack
         STARTTASK_PRIO,
         STARTTASK_PRIO,
         (void *)&StartTask_Stack[0],
         StartTask_STACKSIZE,
         (void *) 0,  
         OS_TASK_OPT_STK_CHK | OS_TASK_OPT_STK_CLR);
         
       
  OSStart();
 
  return 0;
}