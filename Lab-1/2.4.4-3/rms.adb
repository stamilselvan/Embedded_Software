pragma Task_Dispatching_Policy(FIFO_Within_Priorities);

with Ada.Text_IO, Ada.Real_Time, Ada.Integer_Text_IO, delay_time;
use Ada.Text_IO, Ada.Real_Time, Ada.Integer_Text_IO, delay_time;

procedure rms is

   START_TIME : Time := Clock;

   package Duration_IO is new Ada.Text_IO.Fixed_IO(Duration);

   task type Task_Def (User_Def_Period : Integer; Exe_Time : Integer; User_Def_Priority : Integer) is
      pragma Priority(User_Def_Priority);
   end Task_Def;

   task body Task_Def is
      Release : Time_Span := Milliseconds(0);
      Next_Release : Time;
      Period : Time_Span := Milliseconds(User_Def_Period * 100);
   begin
      Next_Release := START_TIME + Release;
      loop
         Put_Line("Executing Task Priority : " & Integer'Image(User_Def_Priority));
         Put("Time_Stamp : ");
         Duration_IO.Put(To_Duration(Clock - START_TIME), 3, 3);
         New_Line;
         delay_time.ExecutionTimes(Exe_Time);
         Put("Time_Stamp : ");
         Duration_IO.Put(To_Duration(Clock - START_TIME), 3, 3);
         Put_Line("");
         Next_Release:= Next_Release + Period;
         delay until Next_Release;
      end loop;

   end Task_Def;

   T1 : Task_Def (3, 1, 20);
   T2 : Task_Def (4, 1, 17);
   T3 : Task_Def (6, 1, 14);
   T4 : Task_Def (9, 2, 11);

begin
   null;
end rms;
