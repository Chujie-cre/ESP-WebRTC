#include <stdio.h>
#include "mqtt_client.hpp"

extern "C"{
void app_main(void)
{
    mqtt_DoNow();
}
}
