

package old is

   task type Over_Load_Detecction(Hyper_Period : Integer ; OLD_Priority : Integer) is
       pragma Priority(OLD_Priority);
   end Over_Load_Detecction;

   task type Watch_Dog_Timer (Hyper_Period_WDT : Integer; WDT_Priority : Integer) is
      pragma Priority(WDT_Priority);
      entry Issue_OK;
   end Watch_Dog_Timer;


end old;