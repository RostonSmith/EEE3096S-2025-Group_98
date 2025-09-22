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
#include <math.h>
#include "stm32f4xx.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef struct {
    uint16_t width;
    uint16_t height;
    uint32_t execution_time_ms;
    uint64_t cpu_cycles;
    uint64_t cycles_per_ms;
    float throughput_pixels_per_sec;
    uint64_t checksum;
    bool fpu_enabled;
    char data_type[10];  // "float", "double", "fixed"
    float speedup_factor;
} FPUTestResult;

typedef struct {
    uint16_t width;
    uint16_t height;
    uint32_t execution_time_ms;
    uint64_t cpu_cycles;
    uint64_t cycles_per_ms;
    float throughput_pixels_per_sec;
    uint64_t checksum;
    bool used_tiling;
    uint16_t tile_count;
} ScalabilityResult;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define MAX_ITER 100
#define SCALE 1000000

// STM32F4 memory constraints
#define AVAILABLE_RAM_BYTES 131072  // ~128KB available
#define MAX_DIRECT_PIXELS 32768     // Allow larger direct processing on F4
#define TILE_SIZE 128               // Larger tiles for better efficiency

// Clock frequency for STM32F4 (120MHz)
#define SYSTEM_CLOCK_FREQ 120000000

// Test configurations
#define NUM_SCALABILITY_TESTS 11
#define NUM_FPU_TESTS 6  // float/double × FPU enabled/disabled × comparison with fixed-point

// Task 1 image sizes for FPU testing
#define NUM_TASK1_SIZES 5
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
// Timing variables - Fixed type consistency
volatile uint32_t start_time = 0;
volatile uint32_t end_time = 0;
volatile uint32_t execution_time_ms = 0;
volatile uint64_t start_cycles = 0;
volatile uint64_t end_cycles = 0;
volatile uint64_t cpu_cycles = 0;
volatile uint64_t cycles_per_ms = 0;  // Fixed typo
volatile float throughput_pixels_per_sec = 0.0f;
volatile uint64_t checksum = 0;

// Task 1 image sizes (from original practical)
const uint16_t task1_widths[NUM_TASK1_SIZES] = {64, 128, 160, 192, 256};
const uint16_t task1_heights[NUM_TASK1_SIZES] = {64, 128, 160, 192, 256};

// Scalability test configurations
const uint16_t scalability_widths[NUM_SCALABILITY_TESTS] = {
    128, 256, 320, 480, 640, 800,    // Direct processing range
    1024, 1280, 1440, 1600,          // Tiled processing
    1920                             // Full HD
};

const uint16_t scalability_heights[NUM_SCALABILITY_TESTS] = {
    128, 256, 240, 270, 360, 450,    // Direct processing range
    576, 720, 810, 900,              // Tiled processing
    1080                             // Full HD
};

// Results storage
ScalabilityResult scalability_results[NUM_SCALABILITY_TESTS];
FPUTestResult fpu_test_results[NUM_FPU_TESTS * NUM_TASK1_SIZES];
uint8_t current_test_index = 0;
uint8_t current_fpu_test_index = 0;

// FPU testing state
bool fpu_available = true;  // Assume FPU is compiled in, test both modes
uint64_t baseline_cycles_fixed = 0;  // For speedup calculations

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
/* USER CODE BEGIN PFP */
// Core Mandelbrot functions - Task 5 focus
uint64_t calculate_mandelbrot_float(int width, int height, int max_iterations);
uint64_t calculate_mandelbrot_double(int width, int height, int max_iterations);
uint64_t calculate_mandelbrot_fixed(int width, int height, int max_iterations);

// Scalability functions (from Task 4)
uint64_t calculate_mandelbrot_direct(int width, int height, int max_iterations);
uint64_t calculate_mandelbrot_tiled(int width, int height, int max_iterations, uint16_t *tile_count);
uint64_t process_mandelbrot_tile(int start_x, int start_y, int tile_width, int tile_height,
                                int full_width, int full_height, int max_iterations);

// Memory management
bool can_process_directly(int width, int height);
uint16_t calculate_tile_size(int width, int height);

// Timing functions - Fixed for STM32F4
void init_timing_system(void);
void start_precise_timing(void);
void stop_precise_timing(void);
void calculate_throughput(int width, int height);

// Test execution
void run_scalability_test(void);
void run_fpu_impact_test(void);
void execute_single_scalability_test(uint8_t test_index);
void execute_fpu_test(uint8_t size_index, const char* data_type, bool simulate_fpu_disabled);

// FPU detection and simulation
bool is_fpu_enabled(void);
float calculate_speedup(uint64_t baseline_cycles, uint64_t test_cycles);

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
  // Initialize timing system - CORRECTED for STM32F4
  init_timing_system();

  // Initialize results
  current_test_index = 0;
  current_fpu_test_index = 0;

  // Check FPU availability
  fpu_available = is_fpu_enabled();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    // Visual indicator: Turn on LED0 to signal testing start
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET);

    // Run Task 5: FPU Impact Testing (primary focus)
    run_fpu_impact_test();

    // Brief pause
    HAL_Delay(2000);

    // Run Task 4: Scalability testing (secondary)
    run_scalability_test();

    // Turn off all LEDs
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_RESET);

    // Wait before next test cycle
    HAL_Delay(15000);

  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration - 120MHz
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

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

  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3
                          |GPIO_PIN_4|GPIO_PIN_5|GPIO_PIN_6|GPIO_PIN_7, GPIO_PIN_RESET);

  GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3
                          |GPIO_PIN_4|GPIO_PIN_5|GPIO_PIN_6|GPIO_PIN_7;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}

/* USER CODE BEGIN 4 */

// CORRECTED: Initialize DWT cycle counter for STM32F4 (guaranteed DWT support)
void init_timing_system(void) {
  // Enable DWT (Data Watchpoint and Trace) unit
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;

  // Enable cycle counter
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

  // Reset cycle counter
  DWT->CYCCNT = 0;
}

void start_precise_timing(void) {
  // Reset and start DWT cycle counter
  DWT->CYCCNT = 0;
  start_cycles = DWT->CYCCNT;
  start_time = HAL_GetTick();
}

void stop_precise_timing(void) {
  // Stop cycle counter first (most precise)
  end_cycles = DWT->CYCCNT;
  cpu_cycles = end_cycles - start_cycles;

  end_time = HAL_GetTick();
  execution_time_ms = end_time - start_time;

  // Calculate cycles per millisecond for analysis
  if (execution_time_ms > 0) {
    cycles_per_ms = cpu_cycles / execution_time_ms;
  } else {
    cycles_per_ms = 0;
  }

  // If execution time is 0ms but we have cycles, calculate more precise time
  if (execution_time_ms == 0 && cpu_cycles > 0) {
    uint32_t execution_time_us = (cpu_cycles * 1000) / (SYSTEM_CLOCK_FREQ / 1000);
    execution_time_ms = (execution_time_us + 500) / 1000;
  }
}

void calculate_throughput(int width, int height) {
  uint32_t total_pixels = width * height;
  
  if (cpu_cycles > 0) {
    float execution_time_seconds = (float)cpu_cycles / (float)SYSTEM_CLOCK_FREQ;
    throughput_pixels_per_sec = (float)total_pixels / execution_time_seconds;
  } else if (execution_time_ms > 0) {
    throughput_pixels_per_sec = (float)total_pixels * 1000.0f / (float)execution_time_ms;
  } else {
    throughput_pixels_per_sec = 0.0f;
  }
}

// Task 5: FPU detection
bool is_fpu_enabled(void) {
  // Check CPACR (Coprocessor Access Control Register) for FPU access
  return ((SCB->CPACR & 0x00F00000) != 0);
}

// Task 5: Calculate speedup factor
float calculate_speedup(uint64_t baseline_cycles, uint64_t test_cycles) {
  if (test_cycles == 0) return 0.0f;
  return (float)baseline_cycles / (float)test_cycles;
}

// Task 5: Mandelbrot using single-precision floats
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

// Task 5: Mandelbrot using double precision
uint64_t calculate_mandelbrot_double(int width, int height, int max_iterations) {
  uint64_t mandelbrot_sum = 0;

  for (int y = 0; y < height; y++) {
    double y0 = ((double)y / (double)height) * 2.0 - 1.0;

    for (int x = 0; x < width; x++) {
      double x0 = ((double)x / (double)width) * 3.5 - 2.5;

      double xi = 0.0, yi = 0.0;
      int iteration = 0;

      while (iteration < max_iterations && (xi*xi + yi*yi) <= 4.0) {
        double tmp = xi*xi - yi*yi + x0;
        yi = 2.0*xi*yi + y0;
        xi = tmp;
        iteration++;
      }

      mandelbrot_sum += iteration;
    }
  }
  return mandelbrot_sum;
}

// Task 5: Mandelbrot using fixed-point arithmetic (baseline for comparison)
uint64_t calculate_mandelbrot_fixed(int width, int height, int max_iterations) {
  uint64_t mandelbrot_sum = 0;

  for (int y = 0; y < height; y++) {
    int64_t y0 = ((int64_t)y * 2000000 / height) - 1000000;

    for (int x = 0; x < width; x++) {
      int64_t x0 = ((int64_t)x * 3500000 / width) - 2500000;

      int64_t xi = 0, yi = 0;
      int iteration = 0;

      while (iteration < max_iterations) {
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

// Task 5: Execute single FPU test
void execute_fpu_test(uint8_t size_index, const char* data_type, bool simulate_fpu_disabled) {
  if (size_index >= NUM_TASK1_SIZES) return;

  uint16_t width = task1_widths[size_index];
  uint16_t height = task1_heights[size_index];

  // Store test parameters
  uint8_t result_index = current_fpu_test_index++;
  if (result_index >= NUM_FPU_TESTS * NUM_TASK1_SIZES) return;

  fpu_test_results[result_index].width = width;
  fpu_test_results[result_index].height = height;
  fpu_test_results[result_index].fpu_enabled = !simulate_fpu_disabled;

  // Copy data type string
  for (int i = 0; i < 9 && data_type[i] != '\0'; i++) {
    fpu_test_results[result_index].data_type[i] = data_type[i];
    fpu_test_results[result_index].data_type[i+1] = '\0';
  }

  // Start timing
  start_precise_timing();

  // Execute appropriate Mandelbrot calculation
  if (data_type[0] == 'f') { // "float"
    checksum = calculate_mandelbrot_float(width, height, MAX_ITER);
  } else if (data_type[0] == 'd') { // "double"
    checksum = calculate_mandelbrot_double(width, height, MAX_ITER);
  } else { // "fixed"
    checksum = calculate_mandelbrot_fixed(width, height, MAX_ITER);
  }

  // Stop timing
  stop_precise_timing();
  calculate_throughput(width, height);

  // Store results
  fpu_test_results[result_index].execution_time_ms = execution_time_ms;
  fpu_test_results[result_index].cpu_cycles = cpu_cycles;
  fpu_test_results[result_index].cycles_per_ms = cycles_per_ms;
  fpu_test_results[result_index].throughput_pixels_per_sec = throughput_pixels_per_sec;
  fpu_test_results[result_index].checksum = checksum;

  // Calculate speedup vs baseline (fixed-point)
  if (baseline_cycles_fixed > 0) {
    fpu_test_results[result_index].speedup_factor = calculate_speedup(baseline_cycles_fixed, cpu_cycles);
  } else {
    fpu_test_results[result_index].speedup_factor = 1.0f;
  }

  // Set baseline for first test (fixed-point)
  if (data_type[0] == 'f' && data_type[1] == 'i' && baseline_cycles_fixed == 0) { // "fixed"
    baseline_cycles_fixed = cpu_cycles;
  }
}

// Task 5: Run complete FPU impact test
void run_fpu_impact_test(void) {
  // Test each image size with different data types
  for (uint8_t size_idx = 0; size_idx < NUM_TASK1_SIZES; size_idx++) {

    // LED indication for progress
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, (size_idx % 2) ? GPIO_PIN_SET : GPIO_PIN_RESET);

    // Baseline: Fixed-point arithmetic (no FPU needed)
    execute_fpu_test(size_idx, "fixed", false);
    HAL_Delay(500);

    // Float with FPU (if available)
    execute_fpu_test(size_idx, "float", false);
    HAL_Delay(500);

    // Double with FPU (if available)
    execute_fpu_test(size_idx, "double", false);
    HAL_Delay(500);

    // Note: Actual FPU disable/enable would require Makefile changes
    // In practice, you would compile with different FPU settings:
    // FPU enabled:  FPU = -mfpu=fpv4-sp-d16 -mfloat-abi=hard
    // FPU disabled: # FPU = (commented out)
  }
}

// Memory management: Check if image can be processed directly
bool can_process_directly(int width, int height) {
  uint32_t total_pixels = width * height;
  return (total_pixels <= MAX_DIRECT_PIXELS);
}

uint16_t calculate_tile_size(int width, int height) {
  if (can_process_directly(width, height)) {
    return 0; // No tiling needed
  }

  if (width >= 1920 && height >= 1080) {
    return 256; // Larger tiles for Full HD
  } else if (width >= 1280 || height >= 720) {
    return 192; // Medium tiles for HD content
  } else {
    return TILE_SIZE; // Default tile size
  }
}

// Direct processing (same as fixed-point for scalability tests)
uint64_t calculate_mandelbrot_direct(int width, int height, int max_iterations) {
  return calculate_mandelbrot_fixed(width, height, max_iterations);
}

uint64_t process_mandelbrot_tile(int start_x, int start_y, int tile_width, int tile_height,
                                int full_width, int full_height, int max_iterations) {
  uint64_t tile_sum = 0;
  
  for (int y = start_y; y < start_y + tile_height && y < full_height; y++) {
    int64_t y0 = ((int64_t)y * 2000000 / full_height) - 1000000;

    for (int x = start_x; x < start_x + tile_width && x < full_width; x++) {
      int64_t x0 = ((int64_t)x * 3500000 / full_width) - 2500000;

      int64_t xi = 0, yi = 0;
      int iteration = 0;

      while (iteration < max_iterations) {
        int64_t xi2 = (xi * xi) / SCALE;
        int64_t yi2 = (yi * yi) / SCALE;
        if (xi2 + yi2 > 4000000) break;

        int64_t tmp = xi2 - yi2 + x0;
        yi = (2 * xi * yi) / SCALE + y0;
        xi = tmp;
        iteration++;
      }
      tile_sum += iteration;
    }
  }
  return tile_sum;
}

uint64_t calculate_mandelbrot_tiled(int width, int height, int max_iterations, uint16_t *tile_count) {
  uint64_t total_sum = 0;
  uint16_t tile_size = calculate_tile_size(width, height);
  uint16_t tiles_processed = 0;
  
  if (tile_size == 0) {
    *tile_count = 1;
    return calculate_mandelbrot_direct(width, height, max_iterations);
  }
  
  for (int tile_y = 0; tile_y < height; tile_y += tile_size) {
    for (int tile_x = 0; tile_x < width; tile_x += tile_size) {
      int current_tile_width = (tile_x + tile_size > width) ? width - tile_x : tile_size;
      int current_tile_height = (tile_y + tile_size > height) ? height - tile_y : tile_size;
      
      uint64_t tile_result = process_mandelbrot_tile(tile_x, tile_y, 
                                                    current_tile_width, current_tile_height,
                                                    width, height, max_iterations);
      total_sum += tile_result;
      tiles_processed++;
      
      if (tiles_processed % 16 == 0) {
        HAL_Delay(1);
      }
    }
  }
  
  *tile_count = tiles_processed;
  return total_sum;
}

void execute_single_scalability_test(uint8_t test_index) {
  if (test_index >= NUM_SCALABILITY_TESTS) return;
  
  uint16_t width = scalability_widths[test_index];
  uint16_t height = scalability_heights[test_index];
  uint16_t tile_count = 0;
  
  scalability_results[test_index].width = width;
  scalability_results[test_index].height = height;
  
  bool use_tiling = !can_process_directly(width, height);
  scalability_results[test_index].used_tiling = use_tiling;
  
  start_precise_timing();
  
  if (use_tiling) {
    checksum = calculate_mandelbrot_tiled(width, height, MAX_ITER, &tile_count);
  } else {
    checksum = calculate_mandelbrot_direct(width, height, MAX_ITER);
    tile_count = 1;
  }
  
  stop_precise_timing();
  calculate_throughput(width, height);
  
  scalability_results[test_index].execution_time_ms = execution_time_ms;
  scalability_results[test_index].cpu_cycles = cpu_cycles;
  scalability_results[test_index].cycles_per_ms = cycles_per_ms;
  scalability_results[test_index].throughput_pixels_per_sec = throughput_pixels_per_sec;
  scalability_results[test_index].checksum = checksum;
  scalability_results[test_index].tile_count = tile_count;
}

void run_scalability_test(void) {
  for (uint8_t i = 0; i < NUM_SCALABILITY_TESTS; i++) {
    execute_single_scalability_test(i);
    
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_SET);
    HAL_Delay(1000);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_RESET);
    HAL_Delay(200);
  }
  
  current_test_index = NUM_SCALABILITY_TESTS;
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
