-- Package: Semaphores

package body Semaphores is
   protected body CountingSemaphore is
      entry Wait
        when Count > 0 is -- Wait if count is  zero
      begin
         Count := Count - 1;
      end Wait;

      entry Signal
        when Count < MaxCount is
      begin
	Count := Count + 1;
      end Signal;
   end CountingSemaphore;
end Semaphores;

