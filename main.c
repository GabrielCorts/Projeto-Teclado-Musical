//Gabriel Cortes Cassiano Pereira        2019000330
//Kevin Lucas Borsato Lino               2020032121
//Mateus Cintra Costa                    2016001960
//Roger Lemos da Silva Oliveira          2017009570

#include "stm32f10x.h "
 // LCD pin definitions

#define LCD_RS 15
#define LCD_EN 12
#define LCD4 8 // PA8
#define LCD5 6 // PA6
#define LCD6 5 // PA5
#define LCD7 11 // PA11

//Botões
#define SW1 12 // PB
#define SW2 13 // PB
#define SW3 14 // PB
//Teclas
#define SW5 5 // PB
#define SW6 4 // PB
#define SW7 3 // PB
#define SW8 3 // PA
#define SW9 4 // PA
#define SW10 8 // PB
#define SW11 9 // PB
#define SW12 11 // PB
#define SW13 10 // PB
#define SW14 7 // PA
#define SW15 15 // PC
#define SW16 14 // PC
#define SW17 13 // PC

// Function prototypes
//----LCD Functions------------------------------------------------
void lcd_init ( void ); // Iniciar o display corretamente
void lcd_command ( unsigned char cmd ); // Enviar comandos
void lcd_data ( unsigned char data ); // Envia dados ( caractere ASCII )
void lcd_print ( char * str ); // Envia strings
void lcd_putValue ( unsigned char value ); // Usada internamente
void delay_us ( uint16_t t ); // Atraso de micro segundos
void delay_ms ( uint16_t t ); // Atraso de mili segundoss

void updateLCD(void); //Atualiza informações do display (oitava e duty cycle)

//----Buzzer functions--------------------------------------------
void buzz (uint16_t fpwm, uint8_t GPIO, uint8_t pin); //Controla acionamento do buzzer
void checkOitava (uint16_t ARR); //Determina qual oitava tocar
void checkTimbre (uint16_t ARR); //Determina qual duty cycle usar (duty cycle define o timbre)
uint16_t calcCiclo(uint16_t ARR); //Calcula o valor a ser armazenado no registro para determinado duty cycle

// Global variables
static uint8_t ciclo0, ciclo = 0; // Armazena o ciclo de trabalho do PWM (0 - 25%, 1 - 50%, 2 - 75%)
static uint8_t oitava0, oitava = 1; // Armazena a oitava das notas (1 - 1a oitava, 2 - 2a oitava)
	
int main () {
	//Desabilitar a interface JTAG
	RCC->APB2ENR |= RCC_APB2ENR_AFIOEN;
	AFIO->MAPR |= AFIO_MAPR_SWJ_CFG_JTAGDISABLE;
	
	RCC->APB2ENR = 0x7C; //Habilita clock das GPIOs
	RCC->APB1ENR |= 1<<1; //Habilita clock do timer 3
	
	//Configura sw14,9,8 como entrada (8) e LCD como saída (3)
	GPIOA->CRL = 0x83388444;
	GPIOA->CRH = 0x34433443;
	
	//Habilita buzzer como saída (B) e sw5,6,7 como entrada (8)
	GPIOB->CRL = 0x4488844B;
	
	//Configura sw1 a 4 e sw8 a 13 como entrada (8)
	GPIOB->CRH = 0x88888888;
	
	//Configura sw15,16,17 como entrada (8)
	GPIOC->CRH = 0x88844444;

	//Configurações do timer 3 - OC3CE = 0, OC3M = 110 (saída ativa com CNT < CCR1), OC3PE = 0, OC3FE = 0, CC3S = 00 (output)
	TIM3->CCMR2 = 0x60;
	TIM3->PSC = 100-1; //fclk = 72e6/100 = 720 KHZ
	TIM3->CR1 = 0x1; //Inicia contagem

	// LCD setup
	lcd_init ();
	delay_ms (100);
	lcd_command(0x1); //Limpa o display
	lcd_command(0xC); //Apago o cursor
	
	//ciclo_25(); //Inicia programa no primeiro timbre
	
	//----Loop Principal-------------------------------------------------------------------------------------------------
	for(;;) {
		//----Checa teclas musicais--------------------------
		if((~GPIOB->IDR) & (1<<SW5))
			buzz(132, 'B', SW5);
		
		if((~GPIOB->IDR) & (1<<SW13))
			buzz(140, 'B', SW13);
		
		if((~GPIOB->IDR) & (1<<SW6))
			buzz(148, 'B', SW6);
		
		if((~GPIOA->IDR) & (1<<SW14))
			buzz(157, 'A', SW14);

		if((~GPIOB->IDR) & (1<<SW7))
			buzz(166, 'B', SW7);
 
		if((~GPIOA->IDR) & (1<<SW8))
			buzz(176, 'A', SW8);
		
		if((~GPIOC->IDR) & (1<<SW15))
			buzz(187, 'C', SW15); 
		
		if((~GPIOA->IDR) & (1<<SW9))
			buzz(198, 'A', SW9);

		if((~GPIOC->IDR) & (1<<SW16))
			buzz(209, 'C', SW16);

		if((~GPIOB->IDR) & (1<<SW10))
			buzz(222, 'B', SW10);

		if((~GPIOC->IDR) & (1<<SW17))
			buzz(235, 'C', SW17);

		if((~GPIOB->IDR) & (1<<SW11))
			buzz(249, 'B', SW11);

		if((~GPIOB->IDR) & (1<<SW12))
			buzz(264, 'B', SW12);
		
		//----Checa oitavas e timbres--------------------------
		checkOitava(0);
		checkTimbre(0);
		
		//----Atualiza display----------------------------------
		updateLCD();
	}

}

//----Principal função que controla acionamento do buzzer----
void	buzz(uint16_t fpwm, uint8_t GPIO, uint8_t pin) {
	uint16_t ARR = (uint16_t) ((720e3/fpwm) - 1); //Cálculo do auto reload register (ARR = fclk/fpwm - 1) - único para cada frequência
	TIM3->ARR = ARR/oitava; //Regula o valor para determinada oitava (1a ou 2a)
	TIM3->CCER |= 1<<8; //Habilita saída do CH3 - CC3E = 1
	
	//Entra no loop até que a tecla seja despressionda (permite mudar timbre e oitava enquanto toca)
	switch(GPIO) {
		case 'A':
			while(~(GPIOA->IDR) & (1<<pin)) {
				checkOitava(ARR);
				checkTimbre(ARR);
				updateLCD();
			}
			break;
			
		case 'B':
			while(~(GPIOB->IDR) & (1<<pin)) {
				checkOitava(ARR);
				checkTimbre(ARR);
				updateLCD();
			}
			break;
			
		case 'C':
			while(~(GPIOC->IDR) & (1<<pin)) {
				checkOitava(ARR);
				checkTimbre(ARR);
				updateLCD();
			}
			break;
	}
	
	TIM3->CCER &= ~(1<<8); //Desabilita saída do CH3 após tecla ser despressionada (desliga o buzzer)
}

//----Determina qual oitava utilizar----
void checkOitava(uint16_t ARR) {
	if(~(GPIOB->IDR) &(1<<SW1))
			oitava = 1; //Primeira oitava
		
	if(~(GPIOB->IDR) &(1<<SW2))
			oitava = 2; //Segunda oitava
	
	//Só carrega o valor de ARR se for diferente de 0
	if(ARR)
		TIM3->ARR = ARR/oitava;
}

//----Determina qual duty cycle utilizar----
void checkTimbre(uint16_t ARR) {
	//Se SW3 for pressionado altera o ciclo de trabalho do PWM (altera o timbre)
	if(~(GPIOB->IDR) &(1<<SW3)) {
		switch(ciclo) {
			case 0:
				ciclo = 1;
				break;
			case 1:
				ciclo = 2;
				break;
			case 2:
				ciclo = 0;
				break;
		}
	}
	
	//Só carrega o valor de CCR3 se ARR for diferente de 0 (ou seja, se algum tecla estiver sendo pressionada)
	if(ARR)
		TIM3->CCR3 = calcCiclo(ARR);
}

//----Calcula para cada duty cycle o valor a ser armazenado no registro----
uint16_t calcCiclo(uint16_t ARR) {
	switch(ciclo) {
		case 0:
			return (25*ARR)/100;
		case 1:
			return (50*ARR)/100;
		case 2:
			return (75*ARR)/100;
		default:
			return (25*ARR)/100;
	}
}



//Atualiza o display apenas quando há mudança de informação (para não atrasar o loop principal)
void updateLCD(void) {
	if(oitava0 == oitava && ciclo0 == ciclo) //Verifica se houve mudança
		return;
	
	oitava0 = oitava;
	ciclo0 = ciclo;
	
	lcd_command(0x80);
	lcd_print("Oitava: ");
	lcd_data(oitava | 0x30);
	lcd_command(0xC0);
	lcd_print("Duty Cycle: ");
	
	switch(ciclo) {
		case 0:
			lcd_data('2');lcd_data('5');
			break;
		case 1:
			lcd_data('5');lcd_data('0');
			break;
		case 2:
			lcd_data('7');lcd_data('5');
			break;
	}
}
//----Funções LCD----------------------------------------------------------------------------
 void lcd_putValue ( unsigned char value ) {
	 uint16_t aux ; // variable to help to build appropriate data out
	 aux = 0x0000 ; // clear aux
	 GPIOA -> BRR = (1 <<5)|(1 <<6)|(1 <<8)|(1 <<11); // clear data lines
	 aux = value & 0xF0 ;
	 aux = aux >>4;
	 GPIOA -> BSRR = (( aux &0x0008 ) <<8) | (( aux &0x0004 ) <<3) | (( aux &0x0002 ) <<5) | (( aux &0x0001 ) <<8);
	 GPIOA -> ODR |= (1 << LCD_EN ); /* EN = 1 for H - to - L pulse */
	 delay_ms (3); /* make EN pulse wider */
	 GPIOA -> ODR &= ~ (1 << LCD_EN ); /* EN = 0 for H - to - L pulse */
	 delay_ms (1); /* wait */
	 GPIOA -> BRR = (1 <<5)|(1 <<6)|(1 <<8)|(1 <<11); // clear data lines
	 aux = 0x0000 ; // clear aux
	 aux = value & 0x0F ;
	 GPIOA -> BSRR = (( aux &0x0008 ) <<8) | (( aux &0x0004 ) <<3) | (( aux & 0x0002 ) <<5) | (( aux &0x0001 ) <<8);
	 GPIOA -> ODR |= (1 << LCD_EN ); /* EN = 1 for H - to - L pulse */
	 delay_ms (3); /* make EN pulse wider */
	 GPIOA -> ODR &= ~(1 << LCD_EN ); /* EN = 0 for H - to - L pulse */
	 delay_ms (1); /* wait */
 }

 void lcd_command ( unsigned char cmd ) {
	 GPIOA -> ODR &= ~ (1 << LCD_RS ); /* RS = 0 for command */
	 lcd_putValue ( cmd );
 }


 void lcd_data ( unsigned char data ){
	 GPIOA -> ODR |= (1 << LCD_RS ); /* RS = 1 for data */
	 lcd_putValue ( data );
 }

 void lcd_print ( char * str ) {
	 unsigned char i = 0;

	 while ( str [ i ] != 0){ /* while it is not end of string */
		 lcd_data ( str [ i ]); /* show str [ i ] on the LCD */
		 i ++;
	 }
 }

 void lcd_init (){
	 delay_ms (15);
	 GPIOA -> ODR &= ~(1 << LCD_EN ); /* LCD_EN = 0 */
	 delay_ms (3); /* wait 3 ms */
	 lcd_command (0x33 ); // lcd init .
	 delay_ms (5);
	 lcd_command (0x32 ); // lcd init .
	 delay_us (3000);
	 lcd_command (0x28 ); // 4 - bit mode , 1 line and 5 x8 charactere set
	 delay_ms (3);
	 lcd_command (0x0e ); // display on , cursor on
	 delay_ms (3);
	 lcd_command (0x01 ); // display clear
	 delay_ms (3);
	 lcd_command (0x06 ); // move right
	 delay_ms (3);
 }

 void delay_us ( uint16_t t ) {
 volatile unsigned long l = 0;
	 for ( uint16_t i = 0; i < t ; i ++){
		 for ( l = 0; l < 6; l ++) {
		 }
	 }
 }

 void delay_ms ( uint16_t t ) {
 volatile unsigned long l = 0;
	 for ( uint16_t i = 0; i < t ; i ++){
		 for ( l = 0; l < 6000; l ++) {
		 }
	 }
 }