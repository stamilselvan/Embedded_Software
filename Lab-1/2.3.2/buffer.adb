with Ada.Text_IO, Ada.Integer_Text_IO;
use Ada.Text_IO, Ada.Integer_Text_IO;

package body Buffer is

   protected body CircularBuffer is

      entry Add_Data(X : Integer)
        when Count /= 4 is -- Check if Buffer is full
      begin
         A(In_Ptr) := X;
         Put("Data Added to Buffer");
         New_Line;
         In_Ptr := In_Ptr + 1;
         Count := Count + 1;
      end Add_Data;

      entry Read_Data
        when Count > 0 is -- Check if Buffer is empty
      begin
         Put("Reading Data");
         New_Line;
         Put((A(Out_Ptr)));
         New_Line;
         New_Line;
         Count := Count - 1;
         Out_Ptr := Out_Ptr + 1;
      end Read_Data;

   end CircularBuffer;

end Buffer;
