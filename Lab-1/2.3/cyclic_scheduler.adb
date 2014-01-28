with Ada.Text_IO, Ada.Integer_Text_IO;
use Ada.Text_IO, Ada.Integer_Text_IO;

with Ada.Real_Time;
use Ada.Real_Time;

with System_Function_Package;
use System_Function_Package;

procedure Cyclic_Scheduler is
   X : Integer := 0; -- Input
   Y : Integer := 0; -- Input
   result_X : Integer; -- Output from System_X
   result_Y : Integer; -- Output from System_Y

   task Source_X;
   task Source_Y;
   task Scheduler;

   task body Source_X is -- Generates Data for Input X
      Start : Time;
      Next_Release : Time;
      Release : Time_Span := Milliseconds(0);
      Period : Time_Span := Milliseconds(1000);
   begin
      Start := Clock;
      Next_Release := Start + Release;
      loop
         delay until Next_Release;
         Next_Release:= Next_Release + Period;
         X := X + 1;
         result_X := System_Function_Package.System_A(X);
         New_Line;
         Put("X Executed ");
         Put(Duration'Image(To_Duration(Release)));
         New_Line;
      end loop;
   end Source_X;

   task body Source_Y is -- Generated Data for Input Y
      Start : Time;
      Next_Release : Time;
      Release: Time_Span := Milliseconds(500);
      Period : Time_Span := Milliseconds(1000);
   begin
      Start := Clock;
      Next_Release := Start + Release;
      loop
         delay until Next_Release;
         Next_Release:= Next_Release + Period;
         Y := Y + 1;
         result_Y := System_Function_Package.System_B(Y);
         New_Line;
         Put("Y Executed :");
         Put(Duration'Image(To_Duration(Release)));
         New_Line;
      end loop;
   end Source_Y;

   task body Scheduler is
      result_Z : Integer; -- Output from System_Z
      Start : Time;
      Next_Release : Time;
      Release : Time_Span := Milliseconds(700);
      Period : Time_Span := Milliseconds(1000);
   begin
     
      Start := Clock;
      Next_Release := Start + Release;
      loop
         delay until Next_Release;
         Next_Release := Next_Release + Period;
         result_Z := System_Function_Package.System_C(result_X, result_Y);
         New_Line;
         Put("Output is Z : ");
         Put(result_Z);
         New_Line;
      end loop;
      
      -- Complete the code for the scheduler...
	  
   end Scheduler;

begin
   null;
end Cyclic_Scheduler;
