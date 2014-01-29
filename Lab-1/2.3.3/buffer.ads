package Buffer is
   N: constant Integer := 4;
   subtype Item is Integer;
   type Index is mod N;
   type Item_Array is array(Index) of Item;
   A: Item_Array;
   In_Ptr, Out_Ptr: Index := 0;
   Count: Integer range 0..N := 0;

   task type CircularBuffer is
      entry Add_Data( X : Integer);
      entry Read_Data;
   end CircularBuffer;
end Buffer;

