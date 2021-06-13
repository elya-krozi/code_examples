#include "mcu_support_package/inc/stm32f10x.h"

#include <app_cfg.h>

#include  <ucos_ii.h>

#include  <cpu.h>
#include  <lib_def.h>
#include  <lib_mem.h>
#include  <lib_str.h>

#include  <bsp.h>

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define APP_ASSERT( statement ) do { if(! (statement) ) { __disable_irq(); while(1){ __BKPT(0xAB); if(0) break;} }  } while(0)
#define NOTHING 0
#define MAXIMUM_BUTTON 12
#define START_FREQUENCY 40
#define PRESCALER 720
#define PERIOD 50000
#define PRESCALER_TIM 7200
#define PERIOD_TIM 10000
#define QUEUE_SIZE 50

static uint8_t button = NOTHING;
static volatile int frequency = 0;
static volatile bool flag = 1;

//для отсчета времени
static const uint32_t hStart = 0; //начальное значение часов
static const uint32_t mStart = 0; //минут
static const uint32_t sStart = 0; //секунд

static const uint8_t hMaxCounter = 24;
static const uint8_t mMaxCounter = 60;
static const uint8_t sMaxCounter = 60;

//очередь
OS_EVENT * queue;
void * queueStorage[ QUEUE_SIZE ] = {0};


/***************************************************************************************************
                                 Стеки для тасков
 ***************************************************************************************************/

static OS_STK App_TaskStartStk[ APP_TASK_START_STK_SIZE ];

static OS_STK App_TaskLedStk[ APP_TASK_LED_STK_SIZE ];

static OS_STK App_TaskScanStk[ APP_TASK_SCAN_STK_SIZE ];

static OS_STK App_TaskDunamicStk[ APP_TASK_DUNAMIC_STK_SIZE ]; 

static OS_STK App_TaskUARTStk[ APP_TASK_UART_STK_SIZE ];

/***************************************************************************************************
                                 Таски - объявления
 ***************************************************************************************************/

static void App_TaskStart( void * p_arg );

static void App_TaskLed( void * p_arg);

static void App_TaskScan( void * p_arg);

static void App_TaskDunamic( void * p_arg);

static void App_TaskUART( void * p_arg);

void TIM2_IRQHandler(void) {
    TIM_ClearFlag(TIM2, TIM_FLAG_Update);
    static uint8_t hCounter = hStart;
    static uint8_t mCounter = mStart;
    static uint8_t sCounter = sStart;
    do {
        if (sCounter < (sMaxCounter - 1)) { 
            sCounter++;
            break;
        }
        sCounter = 0;
    
        if(mCounter < (mMaxCounter-1)) {
            mCounter++;
            break;
        }
        mCounter = 0;
    
        if(hCounter < (hMaxCounter-1)) {
            hCounter++;
            break;
        }
        hCounter = 0;
    } while (0);
    
    char msg[ QUEUE_SIZE ];
    sprintf(msg, "Time: %02i:%02i:%02i\n", hCounter, mCounter, sCounter);
    APP_ASSERT( sizeof(msg) <= sizeof(void *) );
    INT8U err = OS_ERR_NONE;
    //отправляем очереди время
    err = OSQPost( queue, (void *)msg);
    APP_ASSERT(err == OS_ERR_NONE);
}

static void timerHMS (void) {
    BSP_IntInit();
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);
    
    TIM_TimeBaseInitTypeDef tim2;

    __disable_irq();
        
    TIM_TimeBaseStructInit(&tim2);
    tim2.TIM_Prescaler = PRESCALER_TIM - 1;
    tim2.TIM_Period = PERIOD_TIM - 1;
    tim2.TIM_CounterMode = TIM_CounterMode_Up;
    
    TIM_TimeBaseInit(TIM2, &tim2);
    TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);
    BSP_IntVectSet(BSP_INT_ID_TIM1_UP, TIM2_IRQHandler);
    NVIC_EnableIRQ(TIM2_IRQn);
    __enable_irq();
    TIM_Cmd(TIM2,ENABLE);
}

int main( void ) {

    // запрет прерываний и инициализация ОС 
    BSP_IntDisAll();
    OSInit();

    // создание стартового таска - в нем создаются все остальные
    // почему не создавать всё сразу в main'e?
    // возможно, вы хотите создавать их в зависимости от каких-нибудь условий,
    // для которых уже должна работать ОС
    
    INT8U res = OSTaskCreate( 
                    App_TaskStart,     // указатель на таск         
                    NULL,              // параметр вызова (без параметра)
                    &App_TaskStartStk[ APP_TASK_START_STK_SIZE - 1 ], // указатель на стек
                    APP_TASK_START_PRIO // приоритет
                );

    APP_ASSERT( res == OS_ERR_NONE );

    // запуск многозадачности
    OSStart();

    // до сюда выполнение доходить не должно
    return 0;

}

/***************************************************************************************************
                                 Таски
 ***************************************************************************************************/

// стартовый таск
static void App_TaskStart( void * p_arg ) {
    // это чтобы убрать warning о неиспользуемом аргументе
    (void)p_arg;

    //  Фактически - только настройка RCC - 72 МГц от ФАПЧ  (Initialize BSP functions)
    BSP_Init();

    // настройка СисТика
    OS_CPU_SysTickInit();

    // таск для сбора статистики - если он нужен                            
#if (OS_TASK_STAT_EN > 0)
    OSStatInit();
#endif

    // дальше создаются пользовательские таски
    INT8U err = OS_ERR_NONE;
    
    //для светодиода
    err = OSTaskCreate( 
              App_TaskLed,    // указатель на функцию       
              NULL,            // параметр - без параметра              
              &App_TaskLedStk[ APP_TASK_LED_STK_SIZE - 1 ], // указатель на массив для стека 
              APP_TASK_LED_PRIO // приоритет
          );
                
    APP_ASSERT( err == OS_ERR_NONE );

    //для сканирования клавиатуры
    err = OSTaskCreate( 
              App_TaskScan,
              NULL,
              &App_TaskScanStk[ APP_TASK_SCAN_STK_SIZE - 1], 
              APP_TASK_SCAN_PRIO 
          );
                
    APP_ASSERT( err == OS_ERR_NONE );
    
    //для динамика
    err = OSTaskCreate( 
              App_TaskDunamic,
              NULL,
              &App_TaskDunamicStk[ APP_TASK_DUNAMIC_STK_SIZE - 1 ], 
              APP_TASK_DUNAMIC_PRIO 
          );
                
    APP_ASSERT( err == OS_ERR_NONE );
    
    //создаем объект очереди
    queue = OSQCreate( queueStorage, QUEUE_SIZE );
    APP_ASSERT(queue != NULL);
    
    //для UART
    err = OSTaskCreate( 
              App_TaskUART,
              NULL,
              &App_TaskUARTStk[ APP_TASK_UART_STK_SIZE - 1 ], 
              APP_TASK_UART_PRIO 
          );
                
    APP_ASSERT( err == OS_ERR_NONE );

    timerHMS ();
//    // проставляем заглушки для всех прерыавний
//    BSP_IntInit();
    // этот таск больше не нужен 
    err = OSTaskDel (OS_PRIO_SELF);
    APP_ASSERT( err == OS_ERR_NONE );

}

//нажимаем PA.0 -> загорается светодиод PC.8
static void App_TaskLed( void *p_arg) {
    (void)p_arg;
    
    //Setting button РА0
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    
    GPIO_InitTypeDef buttonA;
    buttonA.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    buttonA.GPIO_Pin = GPIO_Pin_0;
//    buttonA.GPIO_Speed = GPIO_Speed_10MHz;
    
    GPIO_Init(GPIOA, &buttonA);
    
    //Setting LED PC8
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
    
    GPIO_InitTypeDef led;
    led.GPIO_Mode = GPIO_Mode_Out_PP;
    led.GPIO_Pin = GPIO_Pin_8;
    led.GPIO_Speed = GPIO_Speed_50MHz;
    
    GPIO_Init(GPIOC, &led );
    
    INT8U err = OS_ERR_NONE;
    
    while(1) {
        if (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_0)) {
            GPIO_SetBits(GPIOC, GPIO_Pin_8);
        } else {
            GPIO_ResetBits(GPIOC, GPIO_Pin_8);
        }
        OSTimeDlyHMSM(0, 0, 0, 50);
        APP_ASSERT( err == OS_ERR_NONE );
    }
}

//сканирует клавиатуру, значение полученной нажатой клавиши отправляет в очередь
//если ничего не нажато, то отправляет 0
static void App_TaskScan( void * p_arg) {
    (void)p_arg;
    __disable_irq();
    //настройка Port A - rows
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    
    GPIO_InitTypeDef rows;
    rows.GPIO_Mode = GPIO_Mode_IPU;
    rows.GPIO_Pin = GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_3 | GPIO_Pin_4;
    rows.GPIO_Speed = GPIO_Speed_50MHz;
    
    GPIO_Init( GPIOA, &rows );
    
    //настройка Port C - columns
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
    
    GPIO_InitTypeDef columns;
    columns.GPIO_Mode = GPIO_Mode_Out_OD;
    columns.GPIO_Pin = GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_3;
    columns.GPIO_Speed = GPIO_Speed_50MHz;
    
    GPIO_Init( GPIOC, &columns );
    __enable_irq();
    
    GPIO_SetBits (GPIOA, GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_3 | GPIO_Pin_4);//give 1 to all lines
    GPIO_SetBits (GPIOC, GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_3);//give 1 to all column
    
    
    while(1) {
        button = NOTHING;
        for (int i = 1; i <= 3; i++) {
            GPIO_ResetBits (GPIOC, (GPIO_Pin_0 << i));//give 0 for the checked column
            OSTimeDlyHMSM(0, 0, 0, 3);
            if (!(GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_1))) {  //Start scanning from 1 button of 1 column,
                button = (1 + (i - 1));
            } else if (!(GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_2))) {
                button = (4 + (i - 1));
            } else if (!(GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_3))) {
                button = (7 + (i - 1));
            } else if (!(GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_4))) { // end scanning * with button 1 of columns. Other columns by analogy
                button = (10 + (i - 1)); //The number of the pressed button is written to the "button". (* = 10; 0 = 11; # = 12)
            }
            GPIO_SetBits(GPIOC, (GPIO_Pin_0 << i));
        }
        //массив для строки вывода
        char msg[ QUEUE_SIZE ];
        switch (button) {
        case 0:
            sprintf(msg, "No button is pressed.\n");
            break;
        case 10:
            sprintf(msg, "Button * is pressed.\n");
            break;
        case 11:
            sprintf(msg, "Button 0 is pressed.\n");
            break;
        case 12:
            sprintf(msg, "Button # is pressed.\n");
            break;
        default:
            sprintf(msg, "Button %i is pressed.\n", button);
        }
        APP_ASSERT( sizeof(msg) <= sizeof(void *) );
        INT8U err = OS_ERR_NONE;
        //отправляем очереди номер кнопки
        err = OSQPost( queue, (void *)msg);
        APP_ASSERT(err == OS_ERR_NONE);

        OSTimeDlyHMSM(0, 0, 0, 10);
    }
}   

//нахождение частоты звучания
static uint32_t findFreguency(void) {
    if (button == NOTHING) {
        frequency = 1;
    } else if (button <= MAXIMUM_BUTTON) { 
        frequency = (START_FREQUENCY + (20*button));
    } 
    return  frequency;
}
//в соответствии с нажатой клавишой генерирует звук определенной частоты
static void App_TaskDunamic( void * p_arg) {
    (void)p_arg;
    __disable_irq();
    //настройка PortB - dynamic
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    
    GPIO_InitTypeDef dynamic;
    dynamic.GPIO_Mode = GPIO_Mode_Out_PP;
    dynamic.GPIO_Pin = GPIO_Pin_5;
    dynamic.GPIO_Speed = GPIO_Speed_50MHz;
    
    GPIO_Init( GPIOB, &dynamic );
    
    //настройка таймера TIM3
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);
    GPIO_PinRemapConfig(GPIO_FullRemap_TIM3, ENABLE);
    
    TIM_TimeBaseInitTypeDef timer;
    TIM_TimeBaseStructInit(&timer);
    timer.TIM_Prescaler = PRESCALER - 1;
    timer.TIM_CounterMode = TIM_CounterMode_Up;
    timer.TIM_Period = PERIOD - 1;
    
    TIM_TimeBaseInit(TIM3, &timer);
    
    //interrupts enabled
    TIM_ITConfig(TIM3, TIM_IT_Update, ENABLE);
    NVIC_EnableIRQ(TIM3_IRQn);
    __enable_irq();
    TIM_Cmd(TIM3, ENABLE); //start timer
    
    while(1) {
        if (findFreguency() == 0) {
            NVIC_DisableIRQ(TIM3_IRQn);
            TIM_Cmd(TIM3,DISABLE);
            TIM3->CNT = 0;
            continue;
        } else {
            //настраиваем таймер в соответствие с полученной частотой
            __disable_irq();
            NVIC_DisableIRQ(TIM3_IRQn);
            TIM_Cmd(TIM3,DISABLE);
            TIM3->CNT = 0;
            TIM3->ARR = PERIOD;
            TIM3->ARR /= frequency;
            TIM_Cmd(TIM3,ENABLE);
            __enable_irq();
            NVIC_EnableIRQ(TIM3_IRQn);
            OSTimeDlyHMSM(0, 0, 0, 10);
        }  
    }        
}

void TIM3_IRQHandler(void) {
    if (TIM_GetITStatus(TIM3, TIM_IT_Update) == SET) {
        TIM_ClearITPendingBit(TIM3, TIM_IT_Update);
        if (flag) { //if nothing is pressed, nothing happens
            GPIO_SetBits(GPIOB, GPIO_Pin_5);
            flag = 0;
        } else {
            GPIO_ResetBits(GPIOB, GPIO_Pin_5);
            flag = 1;
        }
    }
}

//достает сообщение из очереди и отправляет
static void App_TaskUART( void * p_arg) {
    (void)p_arg;
    __disable_irq();
    
    //настройка портов PB.6 и PB.7 для uart
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
    GPIO_PinRemapConfig(GPIO_Remap_USART1, ENABLE);
    
    GPIO_InitTypeDef uart;
    uart.GPIO_Mode = GPIO_Mode_AF_PP;
    uart.GPIO_Pin = GPIO_Pin_6;
    uart.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &uart);
    
    uart.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    uart.GPIO_Pin = GPIO_Pin_7;
    GPIO_Init(GPIOA, &uart);
    
    //настройка uart  
    USART_InitTypeDef usart;
    USART_StructInit (&usart);
    usart.USART_BaudRate = 57600;
    usart.USART_Mode = USART_Mode_Rx | USART_Mode_Tx; 
    usart.USART_StopBits = USART_StopBits_1;
    usart.USART_Parity = USART_Parity_No;
    usart.USART_WordLength = USART_WordLength_8b;
    usart.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    
    USART_Init(USART1, &usart);
    USART_Cmd(USART1, ENABLE);

    __enable_irq();
    
    while(1) {
        INT8U err = OS_ERR_NONE;
        OS_CPU_SR cpu_sr;
//        char copyMsg[ QUEUE_SIZE ];
//        char message = (char)OSQPend( queue, 0, &err);
        CPU_CRITICAL_ENTER(); 
        char message = (char)OSQPend( queue, 0, &err);
        CPU_CRITICAL_EXIT()

        APP_ASSERT( err == OS_ERR_NONE );
        uint32_t sizeMsg = strlen(&message);
        uint8_t* sendMsg = (uint8_t*) message;
        while (sizeMsg) {
            if (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == SET) {
                USART_SendData(USART1, *sendMsg);
                sendMsg++;
                sizeMsg--;
            }
        }
        OSTimeDlyHMSM(0, 0, 0, 10);     
    }
}

#ifdef USE_FULL_ASSERT

// эта функция вызывается, если assert_param обнаружил ошибку
void assert_failed( uint8_t * file, uint32_t line ) {
    /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */

    (void)file;
    (void)line;

    __disable_irq();
    while(1) {
        // это ассемблерная инструкция "отладчик, стой тут"
        // если вы попали сюда, значит вы ошиблись в параметрах вызова функции из SPL.
        // Смотрите в call stack, чтобы найти ее
        __BKPT(0xAB);
    }
}

#endif
