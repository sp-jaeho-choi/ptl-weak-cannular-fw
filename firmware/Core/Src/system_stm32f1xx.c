#include "stm32f1xx.h"

uint32_t SystemCoreClock = 72000000;

const uint8_t AHBPrescTable[16] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 6, 7, 8, 9};
const uint8_t APBPrescTable[8]  = {0, 0, 0, 0, 1, 2, 3, 4};

void SystemInit(void)
{
    // Enable HSI
    RCC->CR |= RCC_CR_HSION;
    
    // Wait for HSI ready
    while((RCC->CR & RCC_CR_HSIRDY) == 0);
    
    // Set HSION bit
    RCC->CR |= (uint32_t)0x00000001;

    // Reset SW, HPRE, PPRE1, PPRE2, ADCPRE and MCO bits
    RCC->CFGR &= (uint32_t)0xF8FF0000;
    
    // Reset HSEON, CSSON and PLLON bits
    RCC->CR &= (uint32_t)0xFEF6FFFF;

    // Reset HSEBYP bit
    RCC->CR &= (uint32_t)0xFFFBFFFF;

    // Reset PLLSRC, PLLXTPRE, PLLMUL and USBPRE/OTGFSPRE bits
    RCC->CFGR &= (uint32_t)0xFF80FFFF;

    // Disable all interrupts and clear pending bits
    RCC->CIR = 0x009F0000;
    
    // Configure the System clock frequency, HCLK, PCLK2 and PCLK1 prescalers
    // HCLK = SYSCLK, PCLK2 = HCLK, PCLK1 = HCLK/2
    // Configure Flash latency
    FLASH->ACR |= FLASH_ACR_LATENCY_2;
    
    // Enable Prefetch Buffer
    FLASH->ACR |= FLASH_ACR_PRFTBE;
    
    // Configure PLL (HSE * 9 = 72 MHz)
    RCC->CFGR |= RCC_CFGR_HPRE_DIV1;  // HCLK = SYSCLK
    RCC->CFGR |= RCC_CFGR_PPRE2_DIV1; // PCLK2 = HCLK
    RCC->CFGR |= RCC_CFGR_PPRE1_DIV2; // PCLK1 = HCLK/2
    
    RCC->CFGR |= RCC_CFGR_PLLSRC_HSE;  // PLL source = HSE
    RCC->CFGR |= RCC_CFGR_PLLMULL9;    // PLL multiplication factor = 9
    
    // Enable HSE
    RCC->CR |= RCC_CR_HSEON;
    
    // Wait till HSE is ready
    while((RCC->CR & RCC_CR_HSERDY) == 0);
    
    // Enable PLL
    RCC->CR |= RCC_CR_PLLON;

    // Wait till PLL is ready
    while((RCC->CR & RCC_CR_PLLRDY) == 0);
    
    // Select PLL as system clock source
    RCC->CFGR &= (uint32_t)((uint32_t)~(RCC_CFGR_SW));
    RCC->CFGR |= RCC_CFGR_SW_PLL;    

    // Wait till PLL is used as system clock source
    while ((RCC->CFGR & (uint32_t)RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL);
    
    // Update SystemCoreClock
    SystemCoreClock = 72000000;
}

void SystemCoreClockUpdate(void)
{
    // Simplified - always 72MHz for this vulnerable firmware
    SystemCoreClock = 72000000;
}