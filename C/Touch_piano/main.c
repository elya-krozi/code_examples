#include "mcu_support_package/inc/stm32f10x.h"
#include "math.h"
#include "stdbool.h"

#define PERIOD 10

static const volatile int frequency[8] = {1047, 1175, 1319, 1397, 1568, 1720, 1976, 2093};
static volatile bool note[8] = {0};
static volatile int counter[8] = {0};

void scan(void) {
    int sumNote = 0;
    for (int i = 0; i < 7; i++) {
        note[i] = 0;
        if (GPIO_ReadInputData(GPIOA) & (1 << (i+1))) {
            note[i] = 1;
            sumNote++;
        }
    }
    note[7] = 0;
    if (GPIO_ReadInputData(GPIOC) & GPIO_Pin_4) {
        note[7] = 1;
        sumNote++;
    }
}

void takeFinalSin(int devider) {
    if (devider == 0) {
        devider = 1;
    }
    float sumSin = 0;
    for (int i = 0; i < 8; i++) {
        sumSin += sin(i*3.14159265/500);
    }
    
    
}

static volatile int bright = 0;
void TIM3_IRQHandler(void) {
//    TIM_SetCompare3(TIM3, bright); //set filling
    TIM_ClearFlag(TIM3, TIM_FLAG_Update);
    for (int i = 0; i < 8; i++) {
        counter[i] = ;
    }
}

void customizationP(void) {
    //Setting dynamic PC6
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
    
    GPIO_InitTypeDef gpioCStruct;
    gpioCStruct.GPIO_Mode = GPIO_Mode_AF_PP;
    gpioCStruct.GPIO_Pin = GPIO_Pin_6;
    gpioCStruct.GPIO_Speed = GPIO_Speed_50MHz;
    
    GPIO_Init( GPIOC, &gpioCStruct );
    
    //Setting buttons РА1..РА7
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    
    GPIO_InitTypeDef gpioAStruct;
    gpioAStruct.GPIO_Mode = GPIO_Mode_IPD;
    gpioAStruct.GPIO_Pin = GPIO_Pin_1 + GPIO_Pin_2 + GPIO_Pin_3 + GPIO_Pin_4 + GPIO_Pin_5 + GPIO_Pin_6 + GPIO_Pin_7;
    
    GPIO_Init( GPIOA, &gpioAStruct );
    
    //and button РС4
    gpioCStruct.GPIO_Mode = GPIO_Mode_IPD;
    gpioCStruct.GPIO_Pin = GPIO_Pin_4;
    
    GPIO_Init( GPIOC, &gpioCStruct );
}

int main(void) {
   customizationP();
    
    //Setting TIM3
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
    GPIO_PinRemapConfig(GPIO_FullRemap_TIM3, ENABLE);
    
    TIM_TimeBaseInitTypeDef timer;
    TIM_TimeBaseStructInit(&timer);
    timer.TIM_Prescaler = 72;
    timer.TIM_CounterMode = TIM_CounterMode_Up;
    timer.TIM_Period = PERIOD;
    
    TIM_TimeBaseInit(TIM3, &timer);
    
    
    //Setting timer channel
    TIM_OCInitTypeDef timChannel;
    
    TIM_OCStructInit(&timChannel);
    timChannel.TIM_OCMode = TIM_OCMode_PWM1;
    timChannel.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OC3Init (TIM3, &timChannel);
    
    //interrupts enabled
    __disable_irq();
    TIM_ITConfig(TIM3, TIM_IT_Update, ENABLE);
    NVIC_EnableIRQ(TIM3_IRQn);
    __enable_irq();
    
    TIM_Cmd(TIM3, ENABLE); //start timer
   
    
    while(1) {
        scan();
    }

    return 0;
}









#ifdef USE_FULL_ASSERT

// эта функция вызывается, если assert_param обнаружил ошибку
void assert_failed(uint8_t * file, uint32_t line)
{ 
    /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
     
    (void)file;
    (void)line;

    __disable_irq();
    while(1)
    {
        // это ассемблерная инструкция "отладчик, стой тут"
        // если вы попали сюда, значит вы ошиблись в параметрах вызова функции из SPL. 
        // Смотрите в call stack, чтобы найти ее
        __BKPT(0xAB);
    }
}

#endif
