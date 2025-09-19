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
#include "stm32f0xx.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define MAX_ITER 100
#define SCALE 1000000  // fixed-point scale factor (1e6)

// Clock frequency for STM32F0 (48MHz)
#define SYSTEM_CLOCK_FREQ 48000000
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */
// Timing variables
volatile uint32_t start_time = 0;
volatile uint32_t end_time = 0;
volatile uint32_t execution_time_ms = 0;

// New variables for Task 3
volatile uint32_t start_cycles = 0;
volatile uint32_t end_cycles = 0;
volatile uint32_t cpu_cycles = 0;
volatile float throughput_pixels_per_sec = 0.0f;

// Results
volatile uint64_t checksum = 0;
volatile uint64_t checksum_results[5];       // [test_sizes]
volatile uint32_t execution_time_results[5]; // [test_sizes]
volatile uint32_t cpu_cycles_results[5];     // [test_sizes] - New for Task 3
volatile float throughput_results[5];        // [test_sizes] - New for Task 3

// Constants for iteration
const int test_sizes[5] = {128, 160, 192, 224, 256};


/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
/* USER CODE BEGIN PFP */
uint64_t calculate_mandelbrot_fixed_point_arithmetic(int width, int height, int max_iterations);
uint64_t calculate_mandelbrot_float(int width, int height, int max_iterations);
uint64_t calculate_mandelbrot_double(int width, int height, int max_iterations);

// New functions for Task 3
void init_timing_system(void);
void start_precise_timing(void);
void stop_precise_timing(void);
void calculate_throughput(int width, int height);

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
  // Initialize timing system for Task 3
  init_timing_system();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    // Visual indicator: Turn on LED0 to signal processing start
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET);

    // Loop through all test sizes
    for (int i = 0; i < 5; i++) {
      int width = test_sizes[i];
      int height = test_sizes[i];

      // Start precise timing for Mandelbrot function only
      start_precise_timing();

      // Run Mandelbrot calculation
      checksum = calculate_mandelbrot_fixed_point_arithmetic(width, height, MAX_ITER);
      // checksum = calculate_mandelbrot_float(width, height, MAX_ITER);
      // checksum = calculate_mandelbrot_double(width, height, MAX_ITER);

      // Stop precise timing
      stop_precise_timing();
      
      // Calculate throughput
      calculate_throughput(width, height);

      // Store results
      checksum_results[i] = checksum;
      execution_time_results[i] = execution_time_ms;
      cpu_cycles_results[i] = cpu_cycles;          
      throughput_results[i] = throughput_pixels_per_sec;

      // Visual indicator: Turn on LED1 to signal processing active
      HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_SET);

      // Keep LEDs ON for 2s after completing each test
      HAL_Delay(2000);
      
      // Turn OFF LED1
      HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_RESET);
    }
    
    // Turn OFF all LEDs
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0|GPIO_PIN_1, GPIO_PIN_RESET);

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

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL12;
  RCC_OscInitStruct.PLL.PREDIV = RCC_PREDIV_DIV1;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
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
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

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

// Task 3: Initialize timing system
void init_timing_system(void) {
  // STM32F0 (Cortex-M0) typically doesn't have DWT
  // Use SysTick as a backup method for more precise timing
  
  #ifdef DWT
  // If DWT is available on this particular F0 variant
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
  DWT->CYCCNT = 0;
  #endif
  
  // SysTick is already configured by HAL_Init(), running at 1kHz
  // Can be used for microsecond-level timing if needed
}

// Task 3: Start precise timing measurement
void start_precise_timing(void) {
  #ifdef DWT
  // Use DWT if available
  DWT->CYCCNT = 0;
  start_cycles = DWT->CYCCNT;
  #else
  // Fallback: estimate cycles from execution time
  start_cycles = 0;
  #endif
  
  start_time = HAL_GetTick();
}

// Task 3: Stop precise timing measurement
void stop_precise_timing(void) {
  end_time = HAL_GetTick();
  execution_time_ms = end_time - start_time;
  
  #ifdef DWT
  // Use DWT if available
  end_cycles = DWT->CYCCNT;
  cpu_cycles = end_cycles - start_cycles;
  #else
  // Fallback: estimate cycles from execution time
  // Convert milliseconds to cycles: time_ms * (clock_freq / 1000)
  cpu_cycles = execution_time_ms * (SYSTEM_CLOCK_FREQ / 1000);
  #endif
}

// Task 3: Calculate throughput in pixels per second
void calculate_throughput(int width, int height) {
  uint32_t total_pixels = width * height;
  
  if (execution_time_ms > 0) {
    // Convert ms to seconds and calculate pixels/second
    throughput_pixels_per_sec = (float)total_pixels * 1000.0f / (float)execution_time_ms;
  } else if (execution_time_ms == 0 && cpu_cycles > 0) {
    // For very fast operations, use cycle count for better precision
    float execution_time_seconds = (float)cpu_cycles / (float)SYSTEM_CLOCK_FREQ;
    throughput_pixels_per_sec = (float)total_pixels / execution_time_seconds;
  } else {
    throughput_pixels_per_sec = 0.0f;
  }
}

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
  __disable_irq();
  while (1)
  {
  }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
}
#endif /* USE_FULL_ASSERT */