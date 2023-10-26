/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2023 STMicroelectronics.
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
#include <string.h>
#include <stdio.h>
#include "stm32h7xx_it.h"
#include "globals.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim5;
DMA_HandleTypeDef hdma_tim2_up;

UART_HandleTypeDef huart3;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_TIM2_Init(void);
static void MX_USB_OTG_HS_USB_Init(void);
static void MX_USART3_UART_Init(void);
static void MX_TIM5_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/*** GLOBAL VARS ***/
uint32_t posLog[LOG_SIZE] = {0};
uint32_t log_position = 0;
uint32_t running_sum = 0;
uint32_t disp_pos = 0;
uint8_t rxBuffer[20]; // serial Rx 10 byte buffer
uint8_t tx_ack[3] = {ACK, '\r', '\n'};
uint8_t tx_bad[3] = {BAD, '\r', '\n'};
uint8_t rxFlag = 0;
int panPos = 0;
int tiltPos = 0;

/*** MOTOR CONTROL FUNCTIONS ***/
void move_tilt_steps(uint32_t delay, uint8_t dir, uint32_t num_steps) {
	HAL_GPIO_WritePin(TILT_EN_GPIO_Port, TILT_EN_Pin, 0);
	HAL_GPIO_WritePin(TILT_DIR_GPIO_Port, TILT_DIR_Pin, dir);
	for(int i=0; i<num_steps; i++) {
		HAL_GPIO_WritePin(TILT_STP_GPIO_Port, TILT_STP_Pin, GPIO_PIN_SET);
		HAL_Delay(delay);
		HAL_GPIO_WritePin(TILT_STP_GPIO_Port, TILT_STP_Pin, GPIO_PIN_RESET);
		HAL_Delay(delay);


	}
	char b[] = "done steps\r\n";
	HAL_UART_Transmit(&huart3, (uint8_t*)b, strlen(b), HAL_MAX_DELAY);

	tiltPos += num_steps * (1 - 2 * dir); // + if dir is 0, else -
//	HAL_GPIO_WritePin(TILT_EN_GPIO_Port, TILT_EN_Pin, 1);

}

void move_tilt_deg(uint32_t degrees, uint8_t dir) {
	move_tilt_steps(TILT_DEFAULT_DELAY, dir, degrees*TILT_DEG_TO_STEPS);
}

void move_pan_steps(uint32_t delay, uint8_t dir, uint32_t num_steps) {
	HAL_GPIO_WritePin(PAN_EN_GPIO_Port, PAN_EN_Pin, 0);
	HAL_GPIO_WritePin(PAN_DIR_GPIO_Port, PAN_DIR_Pin, dir);
	for(int i=0; i<num_steps; i++) {
		HAL_GPIO_WritePin(PAN_STP_GPIO_Port, PAN_STP_Pin, GPIO_PIN_SET);
		HAL_Delay(delay);
		HAL_GPIO_WritePin(PAN_STP_GPIO_Port, PAN_STP_Pin, GPIO_PIN_RESET);
		HAL_Delay(delay);
	}
	char b[] = "done steps\r\n";
	HAL_UART_Transmit(&huart3, (uint8_t*)b, strlen(b), HAL_MAX_DELAY);

	panPos += num_steps * (1 - 2 * dir); // + if dir is 0, else -
//	HAL_GPIO_WritePin(PAN_EN_GPIO_Port, PAN_EN_Pin, 1);

}

void move_pan_deg(uint32_t degrees, uint8_t dir) {
	move_pan_steps(PAN_DEFAULT_DELAY, dir, degrees*PAN_DEG_TO_STEPS);
	char pos[30];
	sprintf(pos, "panPos: %d\r\n", panPos);
	HAL_UART_Transmit(&huart3, (uint8_t*)pos, strlen(pos), HAL_MAX_DELAY);

}

/*** USART Rx HANDLE ***/
void ack(void) {
    HAL_UART_Transmit(&huart3, tx_ack, 3, HAL_MAX_DELAY);
}

void bad(void) {
	HAL_UART_Transmit(&huart3, tx_bad, 3, HAL_MAX_DELAY);
}


void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
	char a[] = "received\r\n";
	char endl[] = "\r\n";
	HAL_UART_Transmit(&huart3, (uint8_t*)a, strlen(a), HAL_MAX_DELAY);
	HAL_UART_Transmit(&huart3, (uint8_t*)rxBuffer, strlen(rxBuffer), HAL_MAX_DELAY);
	HAL_UART_Transmit(&huart3, (uint8_t*)endl, 2, HAL_MAX_DELAY);

	rxFlag = 1;
}

void rx_handle(void) {
	char a[] = "handling\r\n";
	HAL_UART_Transmit(&huart3, (uint8_t*)a, strlen(a), HAL_MAX_DELAY);

	uint32_t amount = (rxBuffer[2]-48) << 24 | (rxBuffer[3]-48) << 16 | (rxBuffer[4]-48) << 8 | (rxBuffer[5]-48);
	char num[30];
	sprintf(num, "%d, %d, %d, %d\r\n", rxBuffer[2], rxBuffer[3], rxBuffer[4], rxBuffer[5]);
	HAL_UART_Transmit(&huart3, (uint8_t*)num, strlen(num), HAL_MAX_DELAY);

	switch(rxBuffer[0]) {
    	case MOVE: ;
			char response[100];
			sprintf(response, "%c%c received this amount: %d\r\n", (int)rxBuffer[0], (int)rxBuffer[1], (int)amount);
			HAL_UART_Transmit(&huart3, (uint8_t*)response, strlen(response), HAL_MAX_DELAY);

			switch(rxBuffer[1]) {
				case UP: ;
					char c[] = "up\r\n";
					HAL_UART_Transmit(&huart3, (uint8_t*)c, strlen(c), HAL_MAX_DELAY);


					move_tilt_deg(amount, DIR_TILT_UP);
					ack();
					break;
				case DOWN: ;
					char d[] = "down\r\n";
					HAL_UART_Transmit(&huart3, (uint8_t*)d, strlen(d), HAL_MAX_DELAY);

					move_tilt_deg(amount, DIR_TILT_DOWN);
					ack();
					break;
				case LEFT: ;
					char e[] = "left\r\n";
					HAL_UART_Transmit(&huart3, (uint8_t*)e, strlen(e), HAL_MAX_DELAY);

					move_pan_deg(amount, DIR_PAN_LEFT);
					ack();
					break;
				case RIGHT: ;
					char f[] = "right\r\n";
					HAL_UART_Transmit(&huart3, (uint8_t*)f, strlen(f), HAL_MAX_DELAY);

					move_pan_deg(amount, DIR_PAN_RIGHT);
					ack();
					break;
				default: ;
					bad();
					break;
			}
			break;

		case PLUNGE: ;
			//retrieve info
			uint32_t brake_pos 	= (rxBuffer[1]-48) << 24 | (rxBuffer[2]-48)  << 16 | (rxBuffer[3]-48)  << 8 | (rxBuffer[4]-48);
			disp_pos			= (rxBuffer[5]-48) << 24 | (rxBuffer[6]-48)  << 16 | (rxBuffer[7]-48)  << 8 | (rxBuffer[8]-48);

			/*reset tracking variables*/
			log_position = 0;
			running_sum = 0;
			memset(posLog, 0, sizeof(posLog));



			/*reset count, make sure timer is disabled first to prevent conflict/weirdness*/
			TIM2->CR1 &= ~TIM_CR1_CEN;
			TIM2->CNT = 0;
			TIM2->CR1 |= TIM_CR1_CEN;
			TIM2->ARR = brake_pos-1; // Counter resets at brake_pos which triggers an interrupt handled in TIM2_IRQHandler (stm32h7xx_it.c)
			TIM5->CR1 |= TIM_CR1_CEN; // Start TIM5 to commence data collection

			uint32_t enc_pos;
			for(int i=0; i<10000; i++) { //temporary to test encoder input
				char response[100] = {0};

				enc_pos = htim2.Instance->CNT;
				sprintf(response, "encoder posn: %d\r\n", (int)enc_pos);
				HAL_UART_Transmit(&huart3, (uint8_t*)response, strlen(response), HAL_MAX_DELAY);

			}
			break;
		case STATUS: ;
			char respns[100] = {0};
			sprintf(respns, "posn");
			HAL_UART_Transmit(&huart3, (uint8_t*)response, strlen(response), HAL_MAX_DELAY);

			break;

		case RELEASE: ;
			//stop brake
			break;
    }
	char b[] = "done handling\r\n";
	HAL_UART_Transmit(&huart3, (uint8_t*)b, strlen(b), HAL_MAX_DELAY);

    rxFlag = 0;
    HAL_UART_Receive_IT(&huart3, rxBuffer, 14);
}

//void DMA1_Init(void) {
//	// Configure DMA for memory-to-memory transfer
//    RCC->AHB1ENR |= RCC_AHB1ENR_DMA1EN; // Enable DMA1 clock
//
//    DMA1_Stream0->CR = (DMA_SxCR_MINC | DMA_SxCR_MSIZE_0 | DMA_SxCR_PSIZE_0); // Adjust data size and increment settings
//    DMA1_Stream0->PAR = (uint32_t)&(TIM2->CNT); // Source address (TIM2's CNT register)
//    DMA1_Stream0->M0AR = (uint32_t)posLog; // Destination address (your data array)
//    DMA1_Stream0->NDTR = LOG_SIZE; // Number of data items to transfer
//
//    DMA1_Stream0->CR |= DMA_SxCR_TCIE; // Enable transfer complete interrupt
//
//    DMA1_Stream0->CR |= DMA_SxCR_EN;    // Enable the DMA stream
//
//	// Configure the trigger source for DMA1_Channel1
//	DMA1_CSELR->CSELR &= ~(0x0F << (4 * 1)); // Clear the trigger selection for channel 1
//	DMA1_CSELR->CSELR |= (your_timer_trigger_source << (4 * 1)); // Set the trigger source
//
//}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */

  /* byte order will be:
   * 0: move(0x01) or plunge (0x02)
   * for plunge:
   * 1/2/3/4: brake posn ticks from current position (must set current to 0)
   * 5/6/7/8: dispense posn ticks from current position (must set current to 0)
   * 9/10/11/12: target speed ticks/sec
   * for move:
   * 1: action
   * 2/3/4/5: amount
   * nothing, enter read loop
   */

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
  MX_DMA_Init();
  MX_TIM2_Init();
  MX_USB_OTG_HS_USB_Init();
  MX_USART3_UART_Init();
  MX_TIM5_Init();
  /* USER CODE BEGIN 2 */

  HAL_UART_Receive_IT(&huart3, rxBuffer, 14); // initialize interrupts

  char msg[] = "program start \r\n";
  HAL_UART_Transmit(&huart3, (uint8_t*)msg, strlen(msg), HAL_MAX_DELAY);

  // Enable the TIM2 global interrupt
  NVIC_SetPriority(TIM2_IRQn, 0); // Adjust priority as needed
  NVIC_EnableIRQ(TIM2_IRQn);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1) {
	  if(rxFlag) rx_handle();
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
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

  /*AXI clock gating */
  RCC->CKGAENR = 0xFFFFFFFF;

  /** Supply configuration update enable
  */
  HAL_PWREx_ConfigSupply(PWR_DIRECT_SMPS_SUPPLY);

  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);

  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI48|RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_BYPASS;
  RCC_OscInitStruct.HSI48State = RCC_HSI48_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 24;
  RCC_OscInitStruct.PLL.PLLP = 2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  RCC_OscInitStruct.PLL.PLLR = 2;
  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_3;
  RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
  RCC_OscInitStruct.PLL.PLLFRACN = 0;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_D3PCLK1|RCC_CLOCKTYPE_D1PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV1;
  RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_Encoder_InitTypeDef sConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 0;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 4294967295;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  sConfig.EncoderMode = TIM_ENCODERMODE_TI1;
  sConfig.IC1Polarity = TIM_ICPOLARITY_RISING;
  sConfig.IC1Selection = TIM_ICSELECTION_DIRECTTI;
  sConfig.IC1Prescaler = TIM_ICPSC_DIV1;
  sConfig.IC1Filter = 0;
  sConfig.IC2Polarity = TIM_ICPOLARITY_RISING;
  sConfig.IC2Selection = TIM_ICSELECTION_DIRECTTI;
  sConfig.IC2Prescaler = TIM_ICPSC_DIV1;
  sConfig.IC2Filter = 0;
  if (HAL_TIM_Encoder_Init(&htim2, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */

}

/**
  * @brief TIM5 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM5_Init(void)
{

  /* USER CODE BEGIN TIM5_Init 0 */

  /* USER CODE END TIM5_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM5_Init 1 */

  /* USER CODE END TIM5_Init 1 */
  htim5.Instance = TIM5;
  htim5.Init.Prescaler = 0;
  htim5.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim5.Init.Period = 4294967295;
  htim5.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim5.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim5) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim5, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim5, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM5_Init 2 */

//  RCC->APB1ENR1 |= RCC_APB1ENR1_TIM5EN; // Enable TIM5 clock
//  TIM5->DIER |= TIM_DIER_UIE; // Enable update interrupt
//  TIM5->ARR = ENC_DMA_POLL_FREQ; // Timebase

  /* USER CODE END TIM5_Init 2 */

}

/**
  * @brief USART3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART3_UART_Init(void)
{

  /* USER CODE BEGIN USART3_Init 0 */

  /* USER CODE END USART3_Init 0 */

  /* USER CODE BEGIN USART3_Init 1 */

  /* USER CODE END USART3_Init 1 */
  huart3.Instance = USART3;
  huart3.Init.BaudRate = 115200;
  huart3.Init.WordLength = UART_WORDLENGTH_8B;
  huart3.Init.StopBits = UART_STOPBITS_1;
  huart3.Init.Parity = UART_PARITY_NONE;
  huart3.Init.Mode = UART_MODE_TX_RX;
  huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart3.Init.OverSampling = UART_OVERSAMPLING_16;
  huart3.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart3.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart3.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart3) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&huart3, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&huart3, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_DisableFifoMode(&huart3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART3_Init 2 */

  /* USER CODE END USART3_Init 2 */

}

/**
  * @brief USB_OTG_HS Initialization Function
  * @param None
  * @retval None
  */
static void MX_USB_OTG_HS_USB_Init(void)
{

  /* USER CODE BEGIN USB_OTG_HS_Init 0 */

  /* USER CODE END USB_OTG_HS_Init 0 */

  /* USER CODE BEGIN USB_OTG_HS_Init 1 */

  /* USER CODE END USB_OTG_HS_Init 1 */
  /* USER CODE BEGIN USB_OTG_HS_Init 2 */

  /* USER CODE END USB_OTG_HS_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Stream0_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Stream0_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream0_IRQn);

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
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOG_CLK_ENABLE();
  __HAL_RCC_GPIOE_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(USB_FS_PWR_EN_GPIO_Port, USB_FS_PWR_EN_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, BRAKE_Pin|TILT_DIR_Pin|TILT_EN_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, TILT_STP_Pin|PAN_STP_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, LD3_Pin|PAN_DIR_Pin|PAN_EN_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : USB_FS_PWR_EN_Pin */
  GPIO_InitStruct.Pin = USB_FS_PWR_EN_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(USB_FS_PWR_EN_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : BRAKE_Pin TILT_DIR_Pin TILT_EN_Pin */
  GPIO_InitStruct.Pin = BRAKE_Pin|TILT_DIR_Pin|TILT_EN_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : TILT_STP_Pin PAN_STP_Pin */
  GPIO_InitStruct.Pin = TILT_STP_Pin|PAN_STP_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : LD3_Pin PAN_DIR_Pin PAN_EN_Pin */
  GPIO_InitStruct.Pin = LD3_Pin|PAN_DIR_Pin|PAN_EN_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : USB_FS_OVCR_Pin */
  GPIO_InitStruct.Pin = USB_FS_OVCR_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(USB_FS_OVCR_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : USB_FS_VBUS_Pin */
  GPIO_InitStruct.Pin = USB_FS_VBUS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(USB_FS_VBUS_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : USB_FS_ID_Pin */
  GPIO_InitStruct.Pin = USB_FS_ID_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Alternate = GPIO_AF10_OTG1_HS;
  HAL_GPIO_Init(USB_FS_ID_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : USB_FS_N_Pin USB_FS_P_Pin */
  GPIO_InitStruct.Pin = USB_FS_N_Pin|USB_FS_P_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : LD2_Pin */
  GPIO_InitStruct.Pin = LD2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LD2_GPIO_Port, &GPIO_InitStruct);

/* USER CODE BEGIN MX_GPIO_Init_2 */
/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

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

#ifdef  USE_FULL_ASSERT
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
