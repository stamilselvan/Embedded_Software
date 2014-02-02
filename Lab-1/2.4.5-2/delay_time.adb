with Ada.Text_IO, Ada.Real_Time;
use Ada.Text_IO, Ada.Real_Time;

package body delay_time is

   procedure ExecutionTimes (Milli_Sec : Integer) is

      package Duration_IO is new Ada.Text_IO.Fixed_IO(Duration);
      package Int_IO is new Ada.Text_IO.Integer_IO(Integer);

      function F(N : Integer) return Integer;

      function F(N : Integer) return Integer is
         X : Integer := 0;
      begin
         for Index in 1..N loop
            for I in 1..5000000 loop
               X := I;
            end loop;
         end loop;
         return X;
      end F;

      Start : Time;
      Dummy : Integer;
      Milli_Sec_Convertion : Integer;
   begin
      Put_Line("Measurement of Execution Times");
      Start := Clock;
      Milli_Sec_Convertion := Milli_Sec * 5; -- Running loop 5 times approximately gives 1ms of execution time
      Dummy := F(Milli_Sec_Convertion);
      Int_IO.Put(Milli_Sec_Convertion, 3);
      Put(" : ");
      Duration_IO.Put(To_Duration(Clock - Start), 3, 3);
      Put_Line("s");
   end ExecutionTimes;

end delay_time;
