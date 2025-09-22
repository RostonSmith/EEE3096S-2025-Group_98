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

// STM32F0 memory constraints - conservative estimates
#define AVAILABLE_RAM_BYTES 6144    // ~6KB available for our use (8KB total - stack/heap)
#define MAX_DIRECT_PIXELS 1536      // Conservative limit for direct processing
#define TILE_SIZE 64                // Tile size for tiled processing

// Clock frequency for STM32F0 (48MHz)
#define SYSTEM_CLOCK_FREQ 48000000

// Scalability test image sizes
#define NUM_SCALABILITY_TESTS 10
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
volatile uint64_t start_cycles = 0;
volatile uint64_t end_cycles = 0;
volatile uint64_t cpu_cycles = 0;
volatile uint64_t cylces_per_ms = 0;
volatile float throughput_pixels_per_sec = 0.0f;
volatile uint64_t checksum = 0;

// Scalability test configurations
const uint16_t scalability_widths[NUM_SCALABILITY_TESTS] = {
    320, 480, 640,     // Direct processing range
    800, 1024, 1280, 1600, 1920  // Tiled processing required
};

const uint16_t scalability_heights[NUM_SCALABILITY_TESTS] = {
    240, 270, 360,     // Direct processing range
    450, 576, 720, 900, 1080   // Tiled processing required
};

// Results storage
ScalabilityResult scalability_results[NUM_SCALABILITY_TESTS];
uint8_t current_test_index = 0;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
/* USER CODE BEGIN PFP */
// Core Mandelbrot functions
uint64_t calculate_mandelbrot_direct(int width, int height, int max_iterations);
uint64_t calculate_mandelbrot_tiled(int width, int height, int max_iterations, uint16_t *tile_count);

// Tile processing functions
uint64_t process_mandelbrot_tile(int start_x, int start_y, int tile_width, int tile_height,
                                int full_width, int full_height, int max_iterations);

// Memory management
bool can_process_directly(int width, int height);
uint16_t calculate_tile_size(int width, int height);

// Timing functions
void init_timing_system(void);
void start_precise_timing(void);
void stop_precise_timing(void);
void calculate_throughput(int width, int height);

// Test execution
void run_scalability_test(void);
void execute_single_scalability_test(uint8_t test_index);

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
  // Initialize timing system
  init_timing_system();

  // Initialize results
  current_test_index = 0;
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    // Visual indicator: Turn on LED0 to signal scalability testing start
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET);

    // Run complete scalability test suite
    run_scalability_test();

    // Turn off all LEDs
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_RESET);

    // Wait before next test cycle
    HAL_Delay(10000);

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

// Initialize timing system
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

// Start precise timing measurement
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

// Stop precise timing measurement
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

// Calculate throughput in pixels per second
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

// Memory management: Check if image can be processed directly
bool can_process_directly(int width, int height) {
  uint32_t total_pixels = width * height;

  // Conservative estimate: each pixel needs minimal memory for coordinate calculation
  // Most processing is done in registers, so we're mainly limited by stack usage
  return (total_pixels <= MAX_DIRECT_PIXELS);
}

// Calculate optimal tile size based on available memory
uint16_t calculate_tile_size(int width, int height) {
  if (can_process_directly(width, height)) {
    return 0; // No tiling needed
  }

  // Use fixed tile size for predictable memory usage
  // Could be optimized based on actual memory measurement
  return TILE_SIZE;
}

// Direct Mandelbrot processing (for smaller images)
uint64_t calculate_mandelbrot_direct(int width, int height, int max_iterations) {
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

// Process a single tile of the image
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

// Tiled Mandelbrot processing (for larger images)
uint64_t calculate_mandelbrot_tiled(int width, int height, int max_iterations, uint16_t *tile_count) {
  uint64_t total_sum = 0;
  uint16_t tile_size = calculate_tile_size(width, height);
  uint16_t tiles_processed = 0;

  if (tile_size == 0) {
    // Should use direct processing
    *tile_count = 1;
    return calculate_mandelbrot_direct(width, height, max_iterations);
  }

  // Process image in tiles
  for (int tile_y = 0; tile_y < height; tile_y += tile_size) {
    for (int tile_x = 0; tile_x < width; tile_x += tile_size) {
      int current_tile_width = (tile_x + tile_size > width) ? width - tile_x : tile_size;
      int current_tile_height = (tile_y + tile_size > height) ? height - tile_y : tile_size;

      uint64_t tile_result = process_mandelbrot_tile(tile_x, tile_y,
                                                    current_tile_width, current_tile_height,
                                                    width, height, max_iterations);
      total_sum += tile_result;
      tiles_processed++;

      // Brief pause to allow for system stability
      if (tiles_processed % 4 == 0) {
        HAL_Delay(1);
      }
    }
  }

  *tile_count = tiles_processed;
  return total_sum;
}

// Execute a single scalability test
void execute_single_scalability_test(uint8_t test_index) {
  if (test_index >= NUM_SCALABILITY_TESTS) return;

  uint16_t width = scalability_widths[test_index];
  uint16_t height = scalability_heights[test_index];
  uint16_t tile_count = 0;

  // Store test parameters
  scalability_results[test_index].width = width;
  scalability_results[test_index].height = height;

  // Determine processing method
  bool use_tiling = !can_process_directly(width, height);
  scalability_results[test_index].used_tiling = use_tiling;

  // Start timing
  start_precise_timing();

  // Execute Mandelbrot calculation
  if (use_tiling) {
    checksum = calculate_mandelbrot_tiled(width, height, MAX_ITER, &tile_count);
  } else {
    checksum = calculate_mandelbrot_direct(width, height, MAX_ITER);
    tile_count = 1;
  }

  // Stop timing
  stop_precise_timing();
  calculate_throughput(width, height);

  // Store results
  scalability_results[test_index].execution_time_ms = execution_time_ms;
  scalability_results[test_index].cpu_cycles = cpu_cycles;
  scalability_results[test_index].cycles_per_ms = cpu_cycles/execution_time_ms;
  scalability_results[test_index].throughput_pixels_per_sec = throughput_pixels_per_sec;
  scalability_results[test_index].checksum = checksum;
  scalability_results[test_index].tile_count = tile_count;
}

// Run complete scalability test suite
void run_scalability_test(void) {
  for (uint8_t i = 0; i < NUM_SCALABILITY_TESTS; i++) {

    execute_single_scalability_test(i);

    // Visual indicator: Turn on LED1 to signal completion
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_SET);

	// Keep LED1 ON for 2 seconds
	HAL_Delay(2000);

  }

  current_test_index = NUM_SCALABILITY_TESTS; // Mark completion
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
