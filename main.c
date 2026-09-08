 #include "stm32f1xx_hal.h"
#include "ssd1306.h"
#include <stdio.h>
#include <string.h>


/* =========================================================
   CẤU HÌNH
   ========================================================= */

#define TOTAL_SLOTS 4


/* =========================================================
   IR SENSOR - 4 Ô ĐỖ XE
   PA0 PA1 PA2 PA3
   LOW = CÓ XE
   ========================================================= */

GPIO_TypeDef* SLOT_PORT[TOTAL_SLOTS] =
{
    GPIOA,
    GPIOA,
    GPIOA,
    GPIOA
};

uint16_t SLOT_PIN[TOTAL_SLOTS] =
{
    GPIO_PIN_0,
    GPIO_PIN_1,
    GPIO_PIN_2,
    GPIO_PIN_3
};


/* =========================================================
   IR SENSOR - CỔNG VÀO / RA
   PA4 = CỔNG VÀO
   PA5 = CỔNG RA

   LOW = CÓ XE
   HIGH = KHÔNG CÓ XE
   ========================================================= */

#define GATE_IN_SENSOR_PORT   GPIOA
#define GATE_IN_SENSOR_PIN    GPIO_PIN_4

#define GATE_OUT_SENSOR_PORT  GPIOA
#define GATE_OUT_SENSOR_PIN   GPIO_PIN_5


/* =========================================================
   HC-SR04
   PB0 = TRIG
   PB1 = ECHO
   ========================================================= */

#define TRIG_PORT GPIOB
#define TRIG_PIN  GPIO_PIN_0

#define ECHO_PORT GPIOB
#define ECHO_PIN  GPIO_PIN_1


/* =========================================================
   BUZZER
   PB10
   ========================================================= */

#define BUZZER_PORT GPIOB
#define BUZZER_PIN  GPIO_PIN_10

#define NGUONG_CANH_BAO_XA 30
#define NGUONG_CANH_BAO_GAN 10


/* =========================================================
   SERVO BARRIER
   PA6 = TIM3_CH1 = BARRIER VÀO
   PA7 = TIM3_CH2 = BARRIER RA

   Timer:
   8 MHz / 8 = 1 MHz
   1 tick = 1 us

   PWM:
   20 ms = 50 Hz
   ========================================================= */

#define SERVO_ANGLE_0       500
#define SERVO_ANGLE_90      1500

#define BARRIER_OPEN_TIME_MS 4000


/* =========================================================
   TRẠNG THÁI BARRIER

   CLOSED:
       Barrier đóng, sẵn sàng nhận xe.

   OPENED:
       Barrier đang mở.
       Sau 4 giây sẽ đóng.

   WAIT_CLEAR:
       Barrier đã đóng nhưng xe vẫn còn ở IR.
       Chờ xe rời khỏi IR rồi mới cho phép
       nhận xe tiếp theo.
   ========================================================= */

typedef enum
{
    BARRIER_CLOSED = 0,
    BARRIER_OPENED,
    BARRIER_WAIT_CLEAR

} BarrierState;


/* Trạng thái barrier */

BarrierState barrierInState =
    BARRIER_CLOSED;

BarrierState barrierOutState =
    BARRIER_CLOSED;


/* Thời điểm barrier mở */

uint32_t barrierInOpenTick = 0;
uint32_t barrierOutOpenTick = 0;


/* =========================================================
   TIMER
   ========================================================= */

TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim3;


/* =========================================================
   FUNCTION PROTOTYPE
   ========================================================= */

void SystemClock_Config(void);

static void MX_GPIO_Init(void);
static void MX_TIM2_Init(void);
static void MX_TIM3_Servo_Init(void);

void Error_Handler(void);


/* =========================================================
   KIỂM TRA Ô ĐỖ
   ========================================================= */

uint8_t IsSlotOccupied(uint8_t slotIndex)
{
    GPIO_PinState state;

    state = HAL_GPIO_ReadPin(
        SLOT_PORT[slotIndex],
        SLOT_PIN[slotIndex]
    );

    if (state == GPIO_PIN_RESET)
    {
        return 1;
    }

    return 0;
}


/* =========================================================
   ĐẾM Ô TRỐNG
   ========================================================= */

uint8_t CountFreeSlots(uint8_t *slotStatus)
{
    uint8_t freeCount = 0;

    for (uint8_t i = 0; i < TOTAL_SLOTS; i++)
    {
        slotStatus[i] =
            IsSlotOccupied(i);

        if (slotStatus[i] == 0)
        {
            freeCount++;
        }
    }

    return freeCount;
}


/* =========================================================
   DELAY MICROSECOND
   TIM2:
   1 tick = 1 us
   ========================================================= */

void DelayUs(uint16_t us)
{
    __HAL_TIM_SET_COUNTER(
        &htim2,
        0
    );

    while (
        __HAL_TIM_GET_COUNTER(&htim2)
        < us
    )
    {
    }
}


/* =========================================================
   HC-SR04
   ========================================================= */

float HCSR04_ReadDistance(void)
{
    uint32_t timeout;
    uint32_t pulseWidth;


    /* -----------------------------------------------------
       TRIG LOW
       ----------------------------------------------------- */

    HAL_GPIO_WritePin(
        TRIG_PORT,
        TRIG_PIN,
        GPIO_PIN_RESET
    );

    DelayUs(2);


    /* -----------------------------------------------------
       TRIG HIGH 10 us
       ----------------------------------------------------- */

    HAL_GPIO_WritePin(
        TRIG_PORT,
        TRIG_PIN,
        GPIO_PIN_SET
    );

    DelayUs(10);


    /* -----------------------------------------------------
       TRIG LOW
       ----------------------------------------------------- */

    HAL_GPIO_WritePin(
        TRIG_PORT,
        TRIG_PIN,
        GPIO_PIN_RESET
    );


    /* -----------------------------------------------------
       CHỜ ECHO HIGH
       ----------------------------------------------------- */

    timeout = 30000;

    while (
        HAL_GPIO_ReadPin(
            ECHO_PORT,
            ECHO_PIN
        ) == GPIO_PIN_RESET
    )
    {
        DelayUs(1);

        if (--timeout == 0)
        {
            return -1;
        }
    }


    /* -----------------------------------------------------
       BẮT ĐẦU ĐO
       ----------------------------------------------------- */

    __HAL_TIM_SET_COUNTER(
        &htim2,
        0
    );

    timeout = 30000;


    /* -----------------------------------------------------
       ĐO THỜI GIAN ECHO HIGH
       ----------------------------------------------------- */

    while (
        HAL_GPIO_ReadPin(
            ECHO_PORT,
            ECHO_PIN
        ) == GPIO_PIN_SET
    )
    {
        if (
            __HAL_TIM_GET_COUNTER(&htim2)
            > timeout
        )
        {
            return -1;
        }
    }


    pulseWidth =
        __HAL_TIM_GET_COUNTER(&htim2);


    /* -----------------------------------------------------
       Khoảng cách cm

       distance = pulseWidth / 58
       ----------------------------------------------------- */

    return pulseWidth / 58.0f;
}


/* =========================================================
   BUZZER
   ========================================================= */

void Buzzer_Update(float distance)
{
    /* Không có vật hoặc ngoài vùng cảnh báo */

    if (
        distance < 0 ||
        distance > NGUONG_CANH_BAO_XA
    )
    {
        HAL_GPIO_WritePin(
            BUZZER_PORT,
            BUZZER_PIN,
            GPIO_PIN_RESET
        );

        return;
    }


    /* -----------------------------------------------------
       Rất gần <= 10 cm
       Buzzer kêu liên tục
       ----------------------------------------------------- */

    if (
        distance <= NGUONG_CANH_BAO_GAN
    )
    {
        HAL_GPIO_WritePin(
            BUZZER_PORT,
            BUZZER_PIN,
            GPIO_PIN_SET
        );

        return;
    }


    /* -----------------------------------------------------
       10 - 30 cm
       Buzzer kêu ngắt quãng
       ----------------------------------------------------- */

    uint32_t beepDelay =
        (uint32_t)(distance * 10);


    HAL_GPIO_WritePin(
        BUZZER_PORT,
        BUZZER_PIN,
        GPIO_PIN_SET
    );

    HAL_Delay(50);


    HAL_GPIO_WritePin(
        BUZZER_PORT,
        BUZZER_PIN,
        GPIO_PIN_RESET
    );

    HAL_Delay(beepDelay);
}


/* =========================================================
   SERVO
   ========================================================= */

void Servo_SetPulse(
    uint32_t channel,
    uint16_t pulseUs
)
{
    /* Giới hạn an toàn */

    if (pulseUs < 500)
    {
        pulseUs = 500;
    }

    if (pulseUs > 2500)
    {
        pulseUs = 2500;
    }


    __HAL_TIM_SET_COMPARE(
        &htim3,
        channel,
        pulseUs
    );
}


/* =========================================================
   KIỂM TRA CÓ XE TẠI CỔNG
   LOW = CÓ XE
   ========================================================= */

uint8_t IsCarAtGate(
    GPIO_TypeDef* port,
    uint16_t pin
)
{
    if (
        HAL_GPIO_ReadPin(
            port,
            pin
        ) == GPIO_PIN_RESET
    )
    {
        return 1;
    }

    return 0;
}


/* =========================================================
   ĐIỀU KHIỂN BARRIER

   STATE 1:
   CLOSED

   Có xe
       ↓
   Mở barrier
       ↓
   OPENED


   STATE 2:
   OPENED

   Đủ 4 giây
       ↓
   Đóng barrier
       ↓
   WAIT_CLEAR


   STATE 3:
   WAIT_CLEAR

   Chờ xe rời cảm biến

   IR HIGH
       ↓
   CLOSED
   ========================================================= */

void Barrier_Update(
    GPIO_TypeDef* sensorPort,
    uint16_t sensorPin,

    uint32_t servoChannel,

    BarrierState *state,

    uint32_t *openTick,

    uint8_t allowOpen
)
{
    uint32_t now =
        HAL_GetTick();


    /* =====================================================
       STATE 1 - BARRIER ĐANG ĐÓNG
       ===================================================== */

    if (
        *state == BARRIER_CLOSED
    )
    {
        /*
           Có xe tại cổng
           và được phép mở
        */

        if (
            IsCarAtGate(
                sensorPort,
                sensorPin
            )
            &&
            allowOpen
        )
        {
            /* MỞ BARRIER */

            Servo_SetPulse(
                servoChannel,
                SERVO_ANGLE_90
            );


            /* Chuyển trạng thái */

            *state =
                BARRIER_OPENED;


            /* Lưu thời điểm mở */

            *openTick =
                now;
        }
    }


    /* =====================================================
       STATE 2 - BARRIER ĐANG MỞ
       ===================================================== */

    else if (
        *state == BARRIER_OPENED
    )
    {
        /*
           Đã mở đủ 4 giây
        */

        if (
            (now - *openTick)
            >= BARRIER_OPEN_TIME_MS
        )
        {
            /* ĐÓNG BARRIER */

            Servo_SetPulse(
                servoChannel,
                SERVO_ANGLE_0
            );


            /*
               Không chuyển về CLOSED ngay.

               Phải chờ xe rời khỏi IR.
            */

            *state =
                BARRIER_WAIT_CLEAR;
        }
    }


    /* =====================================================
       STATE 3 - CHỜ XE RỜI CẢM BIẾN
       ===================================================== */

    else if (
        *state == BARRIER_WAIT_CLEAR
    )
    {
        /*
           IR HIGH = không còn xe
        */

        if (
            !IsCarAtGate(
                sensorPort,
                sensorPin
            )
        )
        {
            /*
               Sẵn sàng nhận xe tiếp theo
            */

            *state =
                BARRIER_CLOSED;
        }
    }
}


/* =========================================================
   OLED
   ========================================================= */

void DisplayParkingStatus(
    uint8_t freeCount,
    uint8_t *slotStatus,
    float distance
)
{
    char line[32];


    /* -----------------------------------------------------
       DÒNG 1
       ----------------------------------------------------- */

    SSD1306_GotoXY(0, 0);

    SSD1306_Puts(
        "BAI DO XE",
        1
    );


    /* -----------------------------------------------------
       DÒNG 3
       ----------------------------------------------------- */

    SSD1306_GotoXY(0, 2);

    sprintf(
        line,
        "TRONG %d SLOT %d",
        freeCount,
        TOTAL_SLOTS
    );

    SSD1306_Puts(
        line,
        1
    );


    /* -----------------------------------------------------
       Ô 1 - Ô 2
       ----------------------------------------------------- */

    SSD1306_GotoXY(0, 4);

    sprintf(
        line,
        "1 %s  2 %s",
        slotStatus[0] ? "X" : "T",
        slotStatus[1] ? "X" : "T"
    );

    SSD1306_Puts(
        line,
        1
    );


    /* -----------------------------------------------------
       Ô 3 - Ô 4
       ----------------------------------------------------- */

    SSD1306_GotoXY(0, 5);

    sprintf(
        line,
        "3 %s  4 %s",
        slotStatus[2] ? "X" : "T",
        slotStatus[3] ? "X" : "T"
    );

    SSD1306_Puts(
        line,
        1
    );


    /* -----------------------------------------------------
       HC-SR04
       ----------------------------------------------------- */

    SSD1306_GotoXY(0, 7);

    if (distance >= 0)
    {
        sprintf(
            line,
            "KC %.0f CM",
            distance
        );

        SSD1306_Puts(
            line,
            1
        );
    }
    else
    {
        SSD1306_Puts(
            "KC -- CM",
            1
        );
    }
}


/* =========================================================
   MAIN
   ========================================================= */

int main(void)
{
    /* -----------------------------------------------------
       HAL
       ----------------------------------------------------- */

    HAL_Init();


    /* -----------------------------------------------------
       CLOCK
       ----------------------------------------------------- */

    SystemClock_Config();


    /* -----------------------------------------------------
       GPIO
       ----------------------------------------------------- */

    MX_GPIO_Init();


    /* -----------------------------------------------------
       TIMER 2
       ----------------------------------------------------- */

    MX_TIM2_Init();


    /* -----------------------------------------------------
       TIMER 3
       ----------------------------------------------------- */

    MX_TIM3_Servo_Init();


    /* -----------------------------------------------------
       START TIMER 2
       ----------------------------------------------------- */

    HAL_TIM_Base_Start(
        &htim2
    );


    /* -----------------------------------------------------
       OLED
       ----------------------------------------------------- */

    SSD1306_Init();

    SSD1306_Clear();


    /* -----------------------------------------------------
       BIẾN
       ----------------------------------------------------- */

    uint8_t slotStatus[TOTAL_SLOTS];

    uint8_t freeCount;

    float distance = -1;

    uint32_t lastDisplayTick = 0;

    const uint32_t
        DISPLAY_INTERVAL_MS = 200;


    /* -----------------------------------------------------
       ĐẢM BẢO 2 BARRIER ĐÓNG KHI KHỞI ĐỘNG
       ----------------------------------------------------- */

    Servo_SetPulse(
        TIM_CHANNEL_1,
        SERVO_ANGLE_0
    );

    Servo_SetPulse(
        TIM_CHANNEL_2,
        SERVO_ANGLE_0
    );


    HAL_Delay(500);


    /* =====================================================
       VÒNG LẶP CHÍNH
       ===================================================== */

    while (1)
    {
        uint32_t now =
            HAL_GetTick();


        /* =================================================
           1. ĐỌC 4 Ô ĐỖ

           PA0
           PA1
           PA2
           PA3
           ================================================= */

        freeCount =
            CountFreeSlots(
                slotStatus
            );


        /* =================================================
           2. BARRIER CỔNG VÀO

           PA4
           PA6

           Chỉ mở nếu còn ô trống
           ================================================= */

        Barrier_Update(

            GATE_IN_SENSOR_PORT,
            GATE_IN_SENSOR_PIN,

            TIM_CHANNEL_1,

            &barrierInState,

            &barrierInOpenTick,

            (freeCount > 0)
        );


        /* =================================================
           3. BARRIER CỔNG RA

           PA5
           PA7

           Cổng ra luôn được phép mở
           ================================================= */

        Barrier_Update(

            GATE_OUT_SENSOR_PORT,
            GATE_OUT_SENSOR_PIN,

            TIM_CHANNEL_2,

            &barrierOutState,

            &barrierOutOpenTick,

            1
        );


        /* =================================================
           4. OLED + HC-SR04 + BUZZER

           Cập nhật mỗi 200 ms
           ================================================= */

        if (
            now - lastDisplayTick
            >= DISPLAY_INTERVAL_MS
        )
        {
            lastDisplayTick =
                now;


            /* Đọc khoảng cách */

            distance =
                HCSR04_ReadDistance();


            /* Buzzer */

            Buzzer_Update(
                distance
            );


            /* OLED */

            SSD1306_Clear();


            DisplayParkingStatus(

                freeCount,

                slotStatus,

                distance
            );
        }
    }
}


/* =========================================================
   GPIO INIT
   ========================================================= */

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef
        GPIO_InitStruct = {0};


    /* -----------------------------------------------------
       ENABLE GPIO CLOCK
       ----------------------------------------------------- */

    __HAL_RCC_GPIOA_CLK_ENABLE();

    __HAL_RCC_GPIOB_CLK_ENABLE();


    /* =====================================================
       PA0 PA1 PA2 PA3
       4 IR Ô ĐỖ

       PA4
       IR CỔNG VÀO

       PA5
       IR CỔNG RA
       ===================================================== */

    GPIO_InitStruct.Pin =
        GPIO_PIN_0 |
        GPIO_PIN_1 |
        GPIO_PIN_2 |
        GPIO_PIN_3 |
        GPIO_PIN_4 |
        GPIO_PIN_5;

    GPIO_InitStruct.Mode =
        GPIO_MODE_INPUT;

    GPIO_InitStruct.Pull =
        GPIO_PULLUP;


    HAL_GPIO_Init(
        GPIOA,
        &GPIO_InitStruct
    );


    /* =====================================================
       PA6
       PA7

       TIM3 CH1
       TIM3 CH2

       SERVO
       ===================================================== */

    GPIO_InitStruct.Pin =
        GPIO_PIN_6 |
        GPIO_PIN_7;

    GPIO_InitStruct.Mode =
        GPIO_MODE_AF_PP;

    GPIO_InitStruct.Speed =
        GPIO_SPEED_FREQ_HIGH;


    HAL_GPIO_Init(
        GPIOA,
        &GPIO_InitStruct
    );


    /* =====================================================
       PB0
       HC-SR04 TRIG

       PB10
       BUZZER
       ===================================================== */

    GPIO_InitStruct.Pin =
        GPIO_PIN_0 |
        GPIO_PIN_10;

    GPIO_InitStruct.Mode =
        GPIO_MODE_OUTPUT_PP;

    GPIO_InitStruct.Pull =
        GPIO_NOPULL;

    GPIO_InitStruct.Speed =
        GPIO_SPEED_FREQ_HIGH;


    HAL_GPIO_Init(
        GPIOB,
        &GPIO_InitStruct
    );


    /* Đảm bảo TRIG LOW */

    HAL_GPIO_WritePin(
        GPIOB,
        GPIO_PIN_0,
        GPIO_PIN_RESET
    );


    /* Đảm bảo BUZZER TẮT */

    HAL_GPIO_WritePin(
        GPIOB,
        GPIO_PIN_10,
        GPIO_PIN_RESET
    );


    /* =====================================================
       PB1
       HC-SR04 ECHO
       ===================================================== */

    GPIO_InitStruct.Pin =
        GPIO_PIN_1;

    GPIO_InitStruct.Mode =
        GPIO_MODE_INPUT;

    GPIO_InitStruct.Pull =
        GPIO_NOPULL;


    HAL_GPIO_Init(
        GPIOB,
        &GPIO_InitStruct
    );


    /* =====================================================
       PB6
       OLED SCL

       PB7
       OLED SDA

       SOFTWARE I2C
       ===================================================== */

    GPIO_InitStruct.Pin =
        GPIO_PIN_6 |
        GPIO_PIN_7;

    GPIO_InitStruct.Mode =
        GPIO_MODE_OUTPUT_OD;

    GPIO_InitStruct.Pull =
        GPIO_PULLUP;

    GPIO_InitStruct.Speed =
        GPIO_SPEED_FREQ_HIGH;


    HAL_GPIO_Init(
        GPIOB,
        &GPIO_InitStruct
    );


    /* I2C IDLE */

    HAL_GPIO_WritePin(
        GPIOB,
        GPIO_PIN_6 |
        GPIO_PIN_7,
        GPIO_PIN_SET
    );
}


/* =========================================================
   TIMER 2
   HC-SR04

   8 MHz / 8 = 1 MHz
   1 tick = 1 us
   ========================================================= */

static void MX_TIM2_Init(void)
{
    __HAL_RCC_TIM2_CLK_ENABLE();


    htim2.Instance =
        TIM2;


    htim2.Init.Prescaler =
        8 - 1;


    htim2.Init.CounterMode =
        TIM_COUNTERMODE_UP;


    htim2.Init.Period =
        0xFFFFFFFF;


    htim2.Init.ClockDivision =
        TIM_CLOCKDIVISION_DIV1;


    if (
        HAL_TIM_Base_Init(
            &htim2
        ) != HAL_OK
    )
    {
        Error_Handler();
    }
}


/* =========================================================
   TIMER 3
   2 SERVO

   CH1 = PA6
   CH2 = PA7

   50 Hz
   20 ms
   ========================================================= */

static void MX_TIM3_Servo_Init(void)
{
    __HAL_RCC_TIM3_CLK_ENABLE();


    htim3.Instance =
        TIM3;


    /*

       8 MHz / 8
       = 1 MHz

       1 tick
       = 1 us

    */

    htim3.Init.Prescaler =
        8 - 1;


    htim3.Init.CounterMode =
        TIM_COUNTERMODE_UP;


    /*

       20 ms

       20000 us

    */

    htim3.Init.Period =
        20000 - 1;


    htim3.Init.ClockDivision =
        TIM_CLOCKDIVISION_DIV1;


    if (
        HAL_TIM_PWM_Init(
            &htim3
        ) != HAL_OK
    )
    {
        Error_Handler();
    }


    TIM_OC_InitTypeDef
        sConfigOC = {0};


    sConfigOC.OCMode =
        TIM_OCMODE_PWM1;


    /* Mặc định barrier đóng */

    sConfigOC.Pulse =
        SERVO_ANGLE_0;


    sConfigOC.OCPolarity =
        TIM_OCPOLARITY_HIGH;


    sConfigOC.OCFastMode =
        TIM_OCFAST_DISABLE;


    /* -----------------------------------------------------
       CHANNEL 1
       PA6
       BARRIER VÀO
       ----------------------------------------------------- */

    if (
        HAL_TIM_PWM_ConfigChannel(
            &htim3,
            &sConfigOC,
            TIM_CHANNEL_1
        ) != HAL_OK
    )
    {
        Error_Handler();
    }


    /* -----------------------------------------------------
       CHANNEL 2
       PA7
       BARRIER RA
       ----------------------------------------------------- */

    if (
        HAL_TIM_PWM_ConfigChannel(
            &htim3,
            &sConfigOC,
            TIM_CHANNEL_2
        ) != HAL_OK
    )
    {
        Error_Handler();
    }


    /* START PWM CH1 */

    HAL_TIM_PWM_Start(
        &htim3,
        TIM_CHANNEL_1
    );


    /* START PWM CH2 */

    HAL_TIM_PWM_Start(
        &htim3,
        TIM_CHANNEL_2
    );
}


/* =========================================================
   SYSTEM CLOCK

   HSI = 8 MHz

   Không dùng PLL.
   ========================================================= */

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef
        RCC_OscInitStruct = {0};

    RCC_ClkInitTypeDef
        RCC_ClkInitStruct = {0};


    /* -----------------------------------------------------
       HSI
       ----------------------------------------------------- */

    RCC_OscInitStruct.OscillatorType =
        RCC_OSCILLATORTYPE_HSI;


    RCC_OscInitStruct.HSIState =
        RCC_HSI_ON;


    RCC_OscInitStruct.HSICalibrationValue =
        RCC_HSICALIBRATION_DEFAULT;


    RCC_OscInitStruct.PLL.PLLState =
        RCC_PLL_NONE;


    if (
        HAL_RCC_OscConfig(
            &RCC_OscInitStruct
        ) != HAL_OK
    )
    {
        Error_Handler();
    }


    /* -----------------------------------------------------
       CLOCK TREE
       ----------------------------------------------------- */

    RCC_ClkInitStruct.ClockType =
        RCC_CLOCKTYPE_HCLK |
        RCC_CLOCKTYPE_SYSCLK |
        RCC_CLOCKTYPE_PCLK1 |
        RCC_CLOCKTYPE_PCLK2;


    RCC_ClkInitStruct.SYSCLKSource =
        RCC_SYSCLKSOURCE_HSI;


    RCC_ClkInitStruct.AHBCLKDivider =
        RCC_SYSCLK_DIV1;


    RCC_ClkInitStruct.APB1CLKDivider =
        RCC_HCLK_DIV1;


    RCC_ClkInitStruct.APB2CLKDivider =
        RCC_HCLK_DIV1;


    if (
        HAL_RCC_ClockConfig(
            &RCC_ClkInitStruct,
            FLASH_LATENCY_0
        ) != HAL_OK
    )
    {
        Error_Handler();
    }
}


/* =========================================================
   ERROR HANDLER
   ========================================================= */

void Error_Handler(void)
{
    __disable_irq();

    while (1)
    {
    }
}