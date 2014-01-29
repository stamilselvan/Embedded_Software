with Ada.Text_IO, Ada.Integer_Text_IO;
use Ada.Text_IO, Ada.Integer_Text_IO;

package body Buffer is

   task body CircularBuffer is
   begin
      loop
         select
            when Count /= 4 => -- Check if Buffer is full
               accept Add_Data (X : in Integer) do
                  A(In_Ptr) := X;
                  Put("Data Added to Buffer");
                  New_Line;
                  In_Ptr := In_Ptr + 1;
                  Count := Count + 1;
               end Add_Data;
         or
            when Count > 0 =>  -- Check if Buffer is empty
               accept Read_Data  do
                  Put("Reading Data");
                  New_Line;
                  Put((A(Out_Ptr)));
                  New_Line;
                  New_Line;
                  Count := Count - 1;
                  Out_Ptr := Out_Ptr + 1;
               end Read_Data;
         or
            terminate;
         end select;
      end loop;
   end CircularBuffer;
end Buffer;
