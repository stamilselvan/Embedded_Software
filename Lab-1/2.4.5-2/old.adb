with Ada.Text_IO, Ada.Real_Time;
use Ada.Text_IO, Ada.Real_Time;

package body old is

   task body Watch_Dog_Timer  is
      Release : Time_Span := Milliseconds(0);
      Next_Release : Time;
      Period : Time_Span := Milliseconds(Hyper_Period_WDT * 100);
      Start_Time : Time := Clock;
   begin
      Next_Release := Start_Time + Release;
      loop

         Next_Release:= Next_Release + Period;
         select
            accept Issue_OK  do
               Put_Line("Success : WDT");
               delay until Next_Release;
            end Issue_OK;
         or
            delay  To_Duration(Milliseconds(Hyper_Period_WDT * 100));
            Put_Line("Warning : Buddy... I'm overloaded :-(");
         end select;
      end loop;
   end Watch_Dog_Timer;

   Task_WDT : Watch_Dog_Timer(36, 25);


   task body Over_Load_Detecction is
      Release : Time_Span := Milliseconds(0);
      Next_Release : Time;
      Period : Time_Span := Milliseconds(Hyper_Period * 100);
      Start_Time : Time := Clock;
   begin
      Next_Release := Start_Time + Release;
      loop
         Put_Line("Calling : WDT : ");
         Task_WDT.Issue_OK;
         Next_Release:= Next_Release + Period;
         delay until Next_Release;
      end loop;
   end Over_Load_Detecction;

   Task_OLD : Over_Load_Detecction(36, 3);

end old;