/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdint.h>
#include <stdbool.h>
#include "stm32f4xx.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define SCALE 1000000  // fixed-point scale factor (1e6)
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
volatile uint32_t start_time = 0;
volatile uint32_t end_time = 0;
volatile uint32_t execution_time = 0;
volatile uint64_t checksum = 0;
volatile uint64_t checksum_results[5][5];       // [max_iter][test_sizes]
volatile uint32_t execution_time_results[5][5]; // [max_iter][test_sizes]

const int test_sizes[5]  = {128, 160, 192, 224, 256};
const int max_iter[5] = {100, 250, 500, 750, 1000};

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
/* USER CODE BEGIN PFP */
uint64_t calculate_mandelbrot_fixed_point_arithmetic(int width, int height, int max_iterations);
uint64_t calculate_mandelbrot_float(int width, int height, int max_iterations);
uint64_t calculate_mandelbrot_double(int width, int height, int max_iterations);


/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  /* USER CODE BEGIN 2 */

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    // Visual indicator: Turn on LED0 to signal processing start
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET);

    // Loop through all max_iter and square sizes
    for (int i = 0; i < 5; i++) {           // max_iter
      for (int j = 0; j < 5; j++) {         // sizes (width = height)
        int width  = test_sizes[j];
        int height = test_sizes[j];
        int max_it = max_iter[i];

        start_time = HAL_GetTick();

        // Run Mandelbrot (can keep all three if you want)
        checksum = calculate_mandelbrot_fixed_point_arithmetic(width, height, max_it);
//        checksum = calculate_mandelbrot_float(width, height, max_it);
//        checksum = calculate_mandelbrot_double(width, height, max_it);

        end_time = HAL_GetTick();
        execution_time = end_time - start_time;

        // Store results
        checksum_results[i][j]       = checksum;
        execution_time_results[i][j] = execution_time;

        // Visual indicator: Turn on LED1 to signal processing active
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_SET);

        HAL_Delay(100); // short pause for visual feedback

        // Turn OFF LED1 between tests
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_RESET);
      }
      HAL_GPIO_WritePin(GPIOB, GPIO_PIN_2, GPIO_PIN_SET);

      HAL_Delay(100);

      HAL_GPIO_WritePin(GPIOB, GPIO_PIN_2, GPIO_PIN_RESET);
    }
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_3, GPIO_PIN_SET);

    // Keep LED0 ON for 2s after completing all tests
    HAL_Delay(2000);

    // Turn OFF all LEDs
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0|GPIO_PIN_3, GPIO_PIN_RESET);

  }
   /* USER CODE END 3 */

}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 15;
  RCC_OscInitStruct.PLL.PLLN = 144;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
/* USER CODE BEGIN MX_GPIO_Init_1 */

/* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3
                          |GPIO_PIN_4|GPIO_PIN_5|GPIO_PIN_6|GPIO_PIN_7, GPIO_PIN_RESET);

  /*Configure GPIO pins : PB0 PB1 PB2 PB3
                           PB4 PB5 PB6 PB7 */
  GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3
                          |GPIO_PIN_4|GPIO_PIN_5|GPIO_PIN_6|GPIO_PIN_7;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

/* USER CODE BEGIN MX_GPIO_Init_2 */

/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
// Mandelbrot using fixed-point integers
uint64_t calculate_mandelbrot_fixed_point_arithmetic(int width, int height, int max_iterations){
    uint64_t mandelbrot_sum = 0;

    for (int y = 0; y < height; y++) {
        // y0 = (y/height)*2.0 - 1.0  (scaled)
        int64_t y0 = ((int64_t)y * 2000000 / height) - 1000000;

        for (int x = 0; x < width; x++) {
            // x0 = (x/width)*3.5 - 2.5  (scaled)
            int64_t x0 = ((int64_t)x * 3500000 / width) - 2500000;

            int64_t xi = 0, yi = 0;
            int iteration = 0;

            while (iteration < max_iterations) {
                // xi*xi + yi*yi <= 4
                int64_t xi2 = (xi * xi) / SCALE;
                int64_t yi2 = (yi * yi) / SCALE;
                if (xi2 + yi2 > 4000000) break;

                int64_t tmp = xi2 - yi2 + x0;
                yi = (2 * xi * yi) / SCALE + y0;
                xi = tmp;
                iteration++;
            }
            mandelbrot_sum += iteration;
        }
    }
    return mandelbrot_sum;

}

// Mandelbrot using single-precision floats
uint64_t calculate_mandelbrot_float(int width, int height, int max_iterations) {
    uint64_t mandelbrot_sum = 0;

    for (int y = 0; y < height; y++) {
        float y0 = ((float)y / (float)height) * 2.0f - 1.0f;

        for (int x = 0; x < width; x++) {
            float x0 = ((float)x / (float)width) * 3.5f - 2.5f;

            float xi = 0.0f, yi = 0.0f;
            int iteration = 0;

            while (iteration < max_iterations && (xi*xi + yi*yi) <= 4.0f) {
                float tmp = xi*xi - yi*yi + x0;
                yi = 2.0f*xi*yi + y0;
                xi = tmp;
                iteration++;
            }

            mandelbrot_sum += iteration;
        }
    }
    return mandelbrot_sum;
}

// Mandelbrot using doubles
uint64_t calculate_mandelbrot_double(int width, int height, int max_iterations){
    uint64_t mandelbrot_sum = 0;

    for (int y = 0; y < height; y++) {
        double y0 = ((double)y / height) * 2.0 - 1.0;
        for (int x = 0; x < width; x++) {
            double x0 = ((double)x / width) * 3.5 - 2.5;

            double xi = 0.0, yi = 0.0;
            int iteration = 0;

            while (iteration < max_iterations && (xi*xi + yi*yi) <= 4.0) {
                double tmp = xi*xi - yi*yi + x0;
                yi = 2*xi*yi + y0;
                xi = tmp;
                iteration++;
            }
            mandelbrot_sum += iteration;
        }
    }
    return mandelbrot_sum;
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
