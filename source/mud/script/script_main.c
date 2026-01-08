
#include "pocketpy.h"
#include <stdio.h>
#include "script_main.h"
#include "mud_profiling.h"


void Script_Init()
{
    py_initialize();

    //py_debugger_waitforattach("127.0.0.1", 6110);
}

void Script_Shutdown()
{
    py_finalize();
}