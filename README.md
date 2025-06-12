# STM32_BLE_Custom_Server_Notify
Here we will be recreating a BLE notification using CubeMx and the IDE.

## General description
So, what will we be doing?

We want to generate a simple counter on the WB5MM and then send it over to our phones every time the counter ticks over (plus print it out on the screen for good measure). Also, ideally, we want to keep our previous server from the previous project – the uart pipe – active while doing so. Sounds rather simple, right?

Well…no. Not at all…

### A few words on Universal Unique Identifiers (UUID)
As mentioned above, we will have multiple services.

All services and characteristics have their own designated “UUID” numbers. These numbers don’t matter much in case we want to generate our own service, but they are essential in case we wish to build our application using existing services or if we want to talk to a client that expects a particular service. All UUID values are standardized and are described in a pdf available on the official Bluetooth website. These identifiers then can be used by the central device to understand, what type of service the peripheral is offering.

Of note, there are a LOT of UUID numbers, and each cover a different service! Of note-of note, these are only the registered official UUIDs and running into some custom nonsense is very likely, especially when dealing with consumer electronics. Official BLE UUID values are always 16-bit wide while custom “vendor specific” ones are 128-bit wide. We will always give only 16-bit values though when defining a UUID, the rest of the 112 bits being generated automatically for us for the custom one. This ensures that the likeliness of two custom services having the same UUID is practically none.

Anyway, in case we wish to communicate with an existing client or peripheral using our BLE solution, we will have to respect the UUID services the existing client will ask for or the existing peripheral, provide. As a matter of fact, communicating with an existing commercial solution will be difficult if not impossible if we don’t know, which UUID services (and UUID characteristics) we need to expect/must generate in our custom BLE solution – i.e., what data is coming our way from the peripheral or must be sent to by us.

In short, UUIDs are ways to standardize the communication protocol between devices using BLE. The service UUID will tell, what the two devices can communicate between each other and the characteristic will tell, what they are communicating with the ongoing packet. From a technical point of view - as far as I can tell - the service UUID will be used to form the packet’s protocol header, while the characteristics UUID will form the packet’s data header.

The service UUIDs are normally found within the advertising data of the peripheral so we can check what they are doing without fully connecting to them. If not, then after connecting, we often can see the available services, their name, UUID and characteristic in a BLE scanner application, such as the ST BLE Toolbox we are using for these projects.

Here, we won’t be going any deeper into UUIDs. We will be just adding the uart_pipe service the UUID of 0x01 and the notification service (called ESS in my code) the UUID of 0x02.

The takeaway is that if we want to communicate with an existing BLE solution/application, the UUID numbers for both he services and the characteristics must be respected to ensure that the right type of data is flying over the BLE bus (don’t put audio data on a handshake).

One more thing: lately I have come over multiple devices there were practically invisible to the ST BLE Toolbox app. These devices could only be connected to through BLE using a third-party application. I am not sure what kind of encoding this is but it does show that not all BLE devices can be communicated to using anything.

### A few words after peaking under the hood of the BLE stack
Something that was never actually clear to me beforehand is that the WPAN runs its own sequencer (same as a scheduler in RTOS) and it is actually the sequencer that is being timed by the mysterious “TimerServer”. To make it a more simplified, we need to set the sequencer IN PARALLEL to the BLE Stack AND all the GATT/GAP goodies. This means that we need the sequencer to run and time everything even if we aren’t directly using it as a manager of custom tasks.

In case we do want to have some timed activities – as is the case with notifications which are time-recurring information packets – those activities MUST be coded within custom tasks managed by the sequencer.

Unfortunately, the nomenclature within the WPAN is very confusing and the GAP/GATT definitions are often organized in the code structure in parallel to the task definitions, making it very difficult to tell, what is for a task and what is for something in GATT.
In general, for a notification, we will need to define a GATT service as a notification and then define a task with its own separate (!) timing profile and then glue the two things together to make the notification do anything. Just doing the GATT service definition – even if it is a notification which simply won’t work without an attached HW-timed task – will define the appropriate “USER_Code” section for us to fill but won’t actually fill the section up for us.

In summary, the BLE stack will not be calling certain WPAN tasks through its characteristics and then the sequencer will time the whole thing and execute the task when it is appropriate….but the other way around: we define the WPAN task which is run by the sequencer, and the task will be updating the characteristic of the notification. We thus will need to put our functions into tasks and then glue them to characteristics and then allow the sequencer to shuffle them around as it sees fit.

### BLE stack code structure sensitivity and timing sensitivity
Arriving to this phase of my evolution of understanding BLE, I am somewhat surprised that the previous project with the uart pipe worked at all. To be fair, while tinkering with it, I did notice that it was ridiculously flimsy, the BLE stack just failing straight up upon minor modifications within the code. Long story short, the sequencer has its own specific code structure that must be complied to allow the sequencer to work properly. Failing to do that will make the sequencer fail and the code freeze without an error message. Again, this is very similar to RTOS.

What does this mean? Well, it means that pretty much all code we write MUST be embedded within the sequencer as either a sequencer-managed init function or a task (more on this later). Also, all functions MUST be made “re-entrant” one way or the other (i.e. either by running in task or by attaching it to the sequencer’s HW clock – the RTC). 

As such, again, similar to RTOS, we do practically nothing within the “main.c” source code (notable exceptions are the hardware initiation functions) and instead have to execute every init within the the app structure, more precisely, in the “app_entry.c” source code “APP_Init” part. 

Regarding timing, the BLE is extremely sensitive to timing – after all, we MUST be doing the frequency hopping every 625 microseconds – and the code we are running on the M4 cannot be allowed to tally long. As a matter of fact, time-consuming run-of-the-mill functions such as “printf” seem to not be “re-entrant” (see RTOS project for what that means), meaning that using them will break the sequencer. Shorter functions, like the “HAL_SPI_write” sequence we have seen in the uart pipe example, might be able to execute fast enough though to not break the code.

Anyway, there are exiting “re-entrant” versions of functions, such as for uart called HW_uart. They are enabled when configuring the WPAN, just go to the “Configuration” part and enable the related function. The “HW” in the name will indicate that they will be clocked on the sequencer’s timer server instead of systick and thus be under the control of the sequencer. We won’t be using custom re-entrant functions in this project though – that will be coming afterwards – but it is good to understand how critical the RTC-run hardware timer is for the sequencer. We will be using HW later to time tasks.

Lastly, it must be mentioned – albeit, not used here – that all IRQ handles MUST be stored within the “app_entry.c” file within the designated part to make them compatible with the sequencer. More on that in the next project.

Lastly-lastly, why was I so critical of my uart pipe project? Because I did a lot of coding within the “main.c” source file. This solution worked while I had only a simple service running but inhibited any kind of additional service to be defined. Once I have reorganized the code, this bug was removed.

### Code structure deep dive
Now that we have a general understanding of the philosophy of the code structure, let’s take a closer look at it.

I also very strongly recommend checking the “Heart Rate Sensor” or “BLE sensor” project description and open one up in the IDE. I will be making connections between these and our custom project to allow an easier comprehension of the structure.
Also, of note, the code is stored both on the WPAN or “app” level as well as within “Middleware” where – conveniently - there also exists a “WPAN” folder. We “should” only be interacting with the app part though.

With these in mind, let’s look at what the wiki on the “Heart Rate Sensor” project (4.4.2 Application initialization) tells us that our code structure:
- 1) main.c – initialize system
- 2) app_entry.c – initialize transport layers and hardware. Configure the peripherals.
- 3) app_ble.c – BLE GAP setup. Starts the BLE stack. 
- 4) svc_ctrl.c - BLE GATT setup or service management. This source code is in “Middleware” and thus should not be modified. It initiates the selected services plus the BLE hardware. We won’t be touching this.
- 5) hrs.c/dis.c (custom_stm.c) – event handler and characteristics update. Both hrs.c and dis.c are within official ST code written for the Heart Reat Sensor and are in “Middleware”.  Our version of this will be the “custom_stm.c” which will be in the WPAN App section.
- 6) hrs_app.c/dis_app.c (custom_app.c) – service notification definition and context initialization. While the Heart Rate Sensor has two apps, we will have only one. We will be doing most of our coding on this layer.

+1) app_conf.h – if we want to have a custom task defined, we will need to add its custom ID to this header, otherwise it won’t be registered by the sequencer. For the Heart Reat Sensor, this is already added to the file.

With this quick overview, we now have a list of culprits to modify for our custom notification to work. But what are to modify?

(Before staring, I suggest to open up the mentioned files from either of the working projects and look for the “User code” indicated sections within the source code to understand, what is going on within the code.)

#### main.c
This is rather straight-forward. Whatever hardware level we wish to activate, we need to do it here, as usual. What we are NOT doing is adding anything else. We also will leave the “while loop” empty except for the “MX_APPE_Process” functions, which will just run the sequencer.

#### app_entry.c
We have the “MX_APPE_Init” function here, which is going to be the “setup” part of our code. As such, we can execute any kind of init action on this layer.

The configuration of the app also occurs here within the “MX_APPE_Config” function, though we don’t need to touch anything in that (it does resetting and HSE tuning for the RTC, for instance, so all app-based actions, plus transport layer setup, timerserver, error activation, etc.). Most of these init and config are done using local functions and will be the same for every WPAN version.

Of note, “HAL_Delay” also defined here as a “wrap” function as well as any other IRQ callback function (both official handle functions - think “HAL_GPIO_EXTI_Callback” - as well as custom handles). This should make these re-entrant. Note that “printf” is not here, meaning that indeed it was not re-entrant.

At the end of the “APPE_Init” function, the stack will be set.

#### app_ble.c
We call the app init in “APP_BLE_Init” user section. At the end of that init function, we start advertising. We also call the “Custom_APP_Init” function, which will be the sequencer’s taks definition (see custom_app.c).

We start any underlying hardware actions attached to the hardware timer here (say, recurring measurement from a sensor) by starting the timer, just after we start advertising. Just a note: all timed actions will be defined according to the hardware timer.

In “SVCCTL_App_Notification”, we need to tell the notification to restart any timed actions as the notification concludes and restart advertising. Similarly, we need to tell the timer to stop when the appropriate action has concluded.

For our simple notification, we won’t be doing any work on this code. We will have to though later in order to make any kind of complex code – say temperature measurement – becoming compliant with the sequencer.

#### app_conf.h
Here the scheduler’s task for the characteristics must be indicated within “CFG_Task_Id_With_HCI_Cmd_t”. The hereby indicated task will be the handled used by the sequencer.

It will also be the one calling by the assigned characteristic – here, our timed notification. Of note, the “Cmd_t” is just an enum, so we merely are calling task numbers eventually. WE can also add some lines to the debug section, though I wasn’t using debug here.

#### custom_app.c
Since we weren’t doing anything but capture incoming data in our uart pipe when the write event was triggered (all set a layer above), this source code is practically devoid of any code within our previous project.

For the notification, we will have to set multiple elements.

Firstly, we have something called a “context” for the service with the notification. This context will be stored in a “Custom_App_Context_t” struct, each context element describing the state of the notification and, by proxy, the task that is attached to it. By definition, the struct will have a “status” and a “connection” element, one indicating if the task should be running while the other indicating if we have connection between the service and the client. We can add additional context variables to struct if we intend to govern more complex tasks, say, because they will be behaving as state machines. For our notification  example, we won’t need any additional context elements than the one generated by CubeMx.

In “Custom_STM_App_Notification”, we will see all the characteristic we will be using across all services, each represented as many times as the number of properties of the characteristic possesses. These sections will be called via the BLE Stack. Practically speaking, they will be the reaction on the device level to the buttons of Read/Write/Notify on the ST BLE app. If the appropriate properties are not allowed for the characteristic though, the buttons will not be available when interrogating the service using the phone. Since the “notify” button is a switch to enable or disable the notification, the notification will have an enable and a disable element in the state machine. In the enable section, we will need to set the notification’s context as “enabled” and start the notification’s task’s HW timer. In the disable section, we remove the context flag and stop the timer.

In “Custom_APP_Notification”, we manage the connect and disconnect handles. We don’t use them for the moment.

In “Custom_APP_Init” is where we are setting up the task for our notification. Firstly, we register it using the ID we have given within “app_conf.h” and define the function we want the task to execute. We then define the HW timer (with a timer id that will be generated by the “HW_TS_Create” function) and give it a callback function. The callback function’s only duty will be to reset the task execution when the timer ticks over. I have also defined a separate app context init function which simply turns off the notification off the bat by resetting the status value.

The task itself will be the custom function we intend to execute, given the notification’s status allows it to occur.

The custom function is very simple: we use the  “Custom_STM_App_Update_Char” to update the value of the characteristic and then print the same value out on the OLED screen. VERY IMPORTANT that we will be updating the entire width of characteristic, so the value must match this width. From the previous project, we know that the width of the characteristic is selected in CubeMx. Here I have given the notification a width of 4 bytes, which means that the counter will need to be 4 bytes wide (i.e. uint32_t).

(Just a note in case someone is following along checking the example projects from ST: they both have two separate services and they will both have their own app section. To make things even more confusing, the env part of the motenv in the BLE_Sensor project is split into a separate source code with the name “app”, which in reality will be just the two separate tasks running within the same service, updating two different characteristics. All in all, “app” means “task”, but CubeMx will not generate a separate “app” source code for each task but instead managed them all under one. I think it would have been better to call the “custom_app.c” as “custom_apps.c” to avoid confusion…)

#### custom_stm.c
We have 3 functions in this source code.

The “Custom_STM_Event_Handler” is where we were using the custom “WRITE event” handler to do data publishing on the OLED. In other words, it is executed when the characteristic is updated and then we extract the value from the characteristic by reading out its “data” element. We can see that happening in the pipe’s event handler. The notification does not have an event associated to it so we won’t see it here. If we were to enable the event generation, the appropriate user code section would have been generated by CubeMx. The event handling that is going on here should always mean resetting a task and NOT calling any functions, again, similar to how RTOS would work. Failing to do that might break the timing…and it is a bit of a miracle it isn’t doing so during our custom uart_pipe, but I digress…

The function “SVCCTL_InitCustomSvc” will set and define the services and their characteristics. It is calling the “svc_ctrl.c” source code. We don’t need to change anything in it, CubeMx has set it up for us properly already.

We have another important function called “Custom_STM_App_Update_Char”. This function will be doing the updating of the characteristic of the service. We will be using this function to update the characteristic of our notification. Thus, it must be called within our task. Mind, this function is service agnostic and must be called for the notification and not for the pipe.

#### Summary
To sum it up (and expanding a bit on what I discussed above):

 “main.c”
-	we initiate the basic hardware and start WPAN
  
“app_conf.h”
-	If we use a custom taks, the task IDs should be put here
  
“app_entry.c”
-	IRQ handles and callbacks go into “FD_WRAP_FUNCTIONS”
-	Code “setup” goes into app_entry.c “APPE_Init”
  
“app_ble.c”
-	We have the advertising elements in the init part (set by CubeMx)
-	We call the init function for the tasks – defining the task and glueing them to the characteristics – in “APP_BLE_Init” function
-	We start any underlying hardware actions attached to the timer in “APP_BLE_Init”.
-	We recurringly initiate the same hardware action in “SVCCTL_App_Notification” BEGIN EVT_DISCONN_COMPLETE section
-	We recurringly reset the timer for the action “SVCCTL_App_Notification” HCI_EVT_LE_CONN_COMPLETE section
  
“Custom_app.c”
-	Within “STM_App_Notification”, we define the characteristics
-	Within “APP_Init”, we define the service tasks with the callback to the HW timer, followed by the context for each service, if they need any (if there is no task needed, no context will be generated)
-	Service task timer callback will be defined here as well
  
“Custom_stm.c”
-	It has the event handler for the characteristics.
-	It will also have the char definition within the ble stack
-	Lastly, it will have the function to update the characteristics

Of note:
-	In general, local function prototypes go to “PFP” and “FD_LOCAL_FUNCTIONS” for each source code
-	In “FD”, we have functions that may be used by lower layers (shared functions), those this I am not sure of (yet)

## Previous relevant projects:
We will be building upon the previous project in the sequence:

- STM32_BLE_Custom_Server_UART
 
As well as heavily harken back to the RTOS project:

- STM32_RTOS
  
In case it is not known, both the BLE_Sensor and Heart Rate examples are available within the firmware package for the WB5MM, similarly where screen drivers are stored.

## To read
Must reads are general custom service setup wiki:

https://wiki.st.com/stm32mcu/wiki/Connectivity:STM32WB_BLE_STM32CubeMX

And the Heart Rate Sensor explanation from the wiki:

https://wiki.st.com/stm32mcu/wiki/Connectivity:STM32WB_HeartRate

For UUID, check the official site for more information:

https://www.bluetooth.com/specifications/assigned-numbers/

## Particularities
### CubeMx
We don’t need to modify much in CubeMx compared to what we were doing for the uart pipe. The only differences are that we are not using events, we are picking “Notify” instead of “write” as a property and we are putting the value length as 4.

Of note, if we intend to keep the uart pipe around, we will need to increase the service number in BLE GATT from 1 to 2 and then give a separate UUID number for the two services. In my code, the two services are called ESS and uart_pipe, though these can be changed as desired.

### Characteristic properties clarification
WRITE will externally update the characteristic of the service. When it happens, we can have an event fire (if enabled in CubeMX). The event will then have its handle in “custom_stm.c”. In the uart pipe, we simply put executable code in the handler, but in reality, we should be instead resetting tasks there.

READ will be practically speaking an RX action from the service to the client. We will need to tell though, what we want to do with that Rx. It does not seem to be just a push-button activated version one-off notification. We will look into this more in the next project.

NOTIFICATION is a recurring publication of the characteristic through BLE done by a HW-timed task. We can enable and disable the notification, albeit this enabling/disabling is clearly stopping the task from updating the characteristic (it stops the connect HW timer element), but I am not sure it is actually physically stopping the publishing itself. Even though we have a notification status element within the app context, I don’t see it being used to stop publishing once started.

### List of things I have learned the hard way
1) Restructuring of the code to be compliant with BLE_Sensor solved the problem with BLE execution/freezing.
2) We set the characteristic property as write, read or notify. Then these will come up as options within “custom_app.c”. Notify will have an enable and disable element to it.
3) Notify is actually a recurring task executed by the sequencer/scheduler. 
4) The notify task is set up by defining a timing to it on the HW, registering the task within the sequencer as a function and then setting the task and starting the timer.
5) The HW timer callback will execute a callback that simply resets the task
6) App context must be set. This will define if the notification starts from the beginning or must be enabled first.
7) The task and the notification/characteristic/app must be set separately. We need a task for any recurring action. Notify is a recurring action.
8) It is the task that drives the notification, not the other way around.

## User guide
We now have a second service running on the WB5MM. It should only be a switch which, when enabled, will start to send a counter to the phone. At the same time, the same number will be written to the WB5’s OLED screen. If the notification is disabled, the counting stops.

Mind, the uart pipe is still there from the previous project and can be used jsut as before.

I am not sharing any of the code that is the same as from the previous project (screen driving, for instance). It is necessary to change the name of the uart pipe service's name from "server1" to "uart_pipe" and the characteristic's name from "char1" to "pipe_str". This will likely wipe the event handler clear, so the uart_pipe action of writing to teh screen will need to be reintroduced to the handler.

## Conclusion
We now have a fully functional WRITE and a NOTIFY server running on the WB5MM, exchanging basic information.

In the next project, we will try to implement a READ on a temperature sensor upon demand from a phone.

