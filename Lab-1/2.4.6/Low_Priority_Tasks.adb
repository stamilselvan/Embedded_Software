with Ada.Text_IO, Ada.Real_Time, delay_time;
use Ada.Text_IO, Ada.Real_Time, delay_time;

package body Low_Priority_Tasks is
   package Duration_IO is new Ada.Text_IO.Fixed_IO(Duration);

   task body Task_Def_Low_Priority  is
      Start_Time : Time := Clock;
   begin
      loop
         Put_Line(" : Executing Low Priority Task : " & Integer'Image(Task_ID));
         Put("Time_Stamp : ");
         Duration_IO.Put(To_Duration(Clock - Start_Time), 3, 3);
         New_Line;
         delay_time.ExecutionTimes(1);
         Put("Time_Stamp : ");
         Duration_IO.Put(To_Duration(Clock - START_TIME), 3, 3);
         Put_Line("");
      end loop;
   end Task_Def_Low_Priority;

   --Low Priority Tasks
   LPT1 : Task_Def_Low_Priority(1, 1);
   LPT2 : Task_Def_Low_Priority(1, 2);
   LPT3 : Task_Def_Low_Priority(1, 3);

end Low_Priority_Tasks;
