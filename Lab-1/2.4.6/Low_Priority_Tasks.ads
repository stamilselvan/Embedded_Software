

package Low_Priority_Tasks is

   task type Task_Def_Low_Priority (LPT_Priority : Integer; Task_ID : Integer) is
       pragma Priority(LPT_Priority);
   end Task_Def_Low_Priority;

end Low_Priority_Tasks;
