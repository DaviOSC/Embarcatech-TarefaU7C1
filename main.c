#include <stdio.h>            
#include <stdlib.h>
#include <time.h>
#include "pico/stdlib.h"      
#include "hardware/adc.h"     
#include "hardware/pio.h"
#include "hardware/i2c.h"
#include "hardware/clocks.h"
#include "inc/ssd1306.h"
#include "inc/font.h"
#include "config.h"
#include "pico/bootrom.h"
#include "pio_matrix.pio.h"

// Definições do display OLED
#define I2C_PORT i2c1
#define endereco 0x3C
#define I2C_SDA 14
#define I2C_SCL 15

// Definições de pinos do joystick
#define VRY_PIN 26  
#define VRX_PIN 27
#define SW_PIN 22

// Definições de pinos dos LEDs
#define LED_PIN_RED 13
#define LED_PIN_GREEN 11

// Definições de pinos dos botões
#define PIN_BUTTON_A 5
#define PIN_BUTTON_B 6

// Definiçãos de pino do buzzer
#define BUZZER_PIN 21

// Definições de pinos da matriz de LEDs
#define OUT_PIN 7
#define NUM_PIXELS 25 // Número de LEDs na matriz

#define DEBOUNCE_TIME_MS 300 // Tempo de debounce em ms

#define EXPRESSION_POS_Y 10 // Posição Y da expressão no display
#define NUMBER_POS_Y 30 // Posição Y do número no display

absolute_time_t last_interrupt_time = {0};
ssd1306_t ssd; 



bool press_button_a = false; // Flag para o botão A
bool is_on = false; // Flag para ligar/desligar o sistema
bool is_serial_mode = false; // Flag para selecionar o modo serial
uint16_t prev_vrx_value = 0; // Valor anterior do eixo X do joystick
uint16_t prev_vry_value = 0; // Valor anterior do eixo Y do joystick
bool vrx_moved = false; // Flag para movimento no eixo X do joystick
bool vry_moved = false; // Flag para movimento no eixo Y do joystick


// Declaração de funções	
static void gpio_irq_handler(uint gpio, uint32_t events);
void play_note(int buzzer, int frequency, int duration);
void play_melody(Note *melody, int buzzer);
void create_expression(char *expression, int *result);
void change_number(uint16_t vry_value, uint16_t vrx_value, int *number, uint16_t vrx_calibration, uint16_t vry_calibration);
uint32_t matrix_rgb(double r, double g, double b);
void pio_drawn(double *desenho, uint32_t valor_led, PIO pio, uint sm, double r, double g, double b);
void correct_answer(uint32_t valor_led, PIO pio, uint sm);
void incorrect_answer(uint32_t valor_led, PIO pio, uint sm);
bool check_answer(int current_number, int result, uint32_t valor_led, PIO pio, uint sm);
void select_mode( uint16_t vrx_value, bool *index, uint16_t vrx_calibration);

int main() { 
  // Inicializa o stdio
  stdio_init_all(); 
  // Inicializa o ADC e os pinos do joystick
  adc_init(); 
  adc_gpio_init(VRX_PIN); 
  adc_gpio_init(VRY_PIN);
  gpio_init(SW_PIN);
  
  // Inicializa os pinos dos botões e LEDs
  gpio_init(PIN_BUTTON_A);
  gpio_init(PIN_BUTTON_B);
  gpio_init(LED_PIN_GREEN);
  gpio_init(LED_PIN_RED);
  gpio_init(BUZZER_PIN);

  // Configura os pinos dos botões e LEDs
  gpio_set_dir(PIN_BUTTON_A, GPIO_IN);
  gpio_set_dir(PIN_BUTTON_B, GPIO_IN);
  gpio_set_dir(SW_PIN, GPIO_IN);
  gpio_set_dir(BUZZER_PIN, GPIO_OUT);
  gpio_set_dir(LED_PIN_GREEN, GPIO_OUT);
  gpio_set_dir(LED_PIN_RED, GPIO_OUT);
  gpio_pull_up(PIN_BUTTON_A);
  gpio_pull_up(PIN_BUTTON_B);
  gpio_pull_up(SW_PIN);

  // Configura a interrupção dos botões 
  gpio_set_irq_enabled_with_callback(PIN_BUTTON_A, GPIO_IRQ_EDGE_FALL, 1, &gpio_irq_handler);
  gpio_set_irq_enabled_with_callback(PIN_BUTTON_B, GPIO_IRQ_EDGE_FALL, 1, &gpio_irq_handler);
  gpio_set_irq_enabled_with_callback(SW_PIN, GPIO_IRQ_EDGE_FALL, 1, &gpio_irq_handler);
  
  i2c_init(I2C_PORT, 400 * 1000); // Inicializa o barramento I2C
  gpio_set_function(I2C_SDA, GPIO_FUNC_I2C); // Configura o pino SDA
  gpio_set_function(I2C_SCL, GPIO_FUNC_I2C); // Configura o pino SCL
  gpio_pull_up(I2C_SDA); // Habilita o pull-up no pino SDA
  gpio_pull_up(I2C_SCL); // Habilita o pull-up no pino SCL
  
  // Configurações da PIO
  PIO pio = pio0; 
  uint32_t valor_led;
  uint sm = pio_claim_unused_sm(pio, true);
  uint offset = pio_add_program(pio, &pio_matrix_program);
  pio_matrix_program_init(pio, sm, offset, OUT_PIN);
  pio_drawn(NULL, valor_led, pio, sm, 0, 0, 0);// Inicializa a matriz de LEDs com todos os LEDs desligados


  // Inicializa o display OLED
  ssd1306_init(&ssd, WIDTH, HEIGHT, false, endereco, I2C_PORT);
  ssd1306_config(&ssd);
  ssd1306_send_data(&ssd);

  // Lê os valores iniciais do joystick para calibração
  adc_select_input(0);  
  uint16_t vry_value = adc_read(); 
  adc_select_input(1);
  uint16_t vrx_value = adc_read();
  
  // Calibração do joystick
  uint16_t vrx_calibration = vrx_value;
  uint16_t vry_calibration = vry_value;
  prev_vrx_value = vrx_calibration;
  prev_vry_value = vry_calibration;

  // Seleciona o canal 2 do ADC, lê o valor analógico e usa esse valor como semente para o gerador de números aleatórios.
  adc_select_input(2); 
  uint16_t seed = adc_read();
  srand(seed);

  char expression[20]; // Variável para armazenar a expressão matemática
  int result; // Variável para armazenar o resultado da expressão
  int current_number = 0;// Variável para armazenar o número digitado ou selecionado
  char current_number_str[10];// Variável para armazenar o número digitado ou selecionado como string

  int selection_index = 0;// Variável para armazenar o índice de seleção do modo
  while(true)
  {
    // Lê os valores do joystick no eixo X
    adc_select_input(1);
    vrx_value = adc_read();

    ssd1306_fill(&ssd, 0);
    ssd1306_rect(&ssd, 3, 3, 122, 60, 1, 0);
    ssd1306_draw_string(&ssd, "Para iniciar", 10, 10);
    ssd1306_draw_string(&ssd, "Selecione o", 10, 20);
    ssd1306_draw_string(&ssd, "Modo", 10, 30);

    // Desenhar duas áreas retangulares distintas para indicar o modo à ser selecionado
    if (!is_serial_mode)
    {
      ssd1306_rect(&ssd, 47, 7, 52, 13, 1, 0); // Retângulo preenchido
    }
    else if (is_serial_mode)
    {
      ssd1306_rect(&ssd, 47, 67, 52, 13, 1, 0); // Retângulo preenchido
    }

    ssd1306_draw_string(&ssd, "Normal", 10, 50);
    ssd1306_draw_string(&ssd, "Serial", 70, 50);

    ssd1306_send_data(&ssd);

    // Sistema para selecionar o serial mode ou normal mode
    select_mode(vrx_value, &is_serial_mode, vrx_calibration);
    while (is_on)
    {
      bool new_expression = false;// Flag para indicar se uma nova expressão foi gerada
      create_expression(expression, &result); // Gera uma nova expressão matemática
      current_number = 0; // Reseta o número digitado ou selecionado
      ssd1306_fill(&ssd, 0); 
      while (!new_expression && is_on && is_serial_mode)
      {
        // Modo serial
        int len = strlen(current_number_str); // Calcula o comprimento da string do número
        int number_pos_x = (WIDTH - len * 6) / 2; // Posição X do número no display de acordo com o comprimento da string
        int len_expression = strlen(expression); // Calcula o comprimento da string da expressão
        int expression_pos_x = (100 - len_expression * 6) / 2;  // Posição X da expressão no display de acordo com o comprimento da string
        ssd1306_rect(&ssd, 3, 3, 122, 60, 1, 0);
        ssd1306_draw_string(&ssd, expression, expression_pos_x, EXPRESSION_POS_Y);
        ssd1306_send_data(&ssd);
        
        char input[30]; 
        printf("Digite a resposta:\n");
        scanf("%s", input);
        // Verifica se o usuário deseja sair do modo serial
        if(strcmp(input, "exit") == 0)
        {
          is_on = false;
          break;
        }

        current_number  = atoi(input);// Converte a string para um número inteiro
        printf("Número escolhido: %d\n", current_number);
        
        sprintf(current_number_str, "%d", current_number); // Converte o número para uma string
        ssd1306_draw_string(&ssd, current_number_str, number_pos_x, NUMBER_POS_Y);
        ssd1306_send_data(&ssd);

        new_expression = check_answer(current_number, result, valor_led, pio, sm); // Verifica a resposta do usuário 
        ssd1306_fill(&ssd, 0);
      }
      while (!new_expression && is_on && !is_serial_mode)
      {
        // Modo normal
        // Lê os valores do joystick nos eixos X e Y
        ssd1306_fill(&ssd, 0);
        adc_select_input(0);
        vry_value = adc_read();
        adc_select_input(1);
        vrx_value = adc_read();
        
        // Muda o número selecionado mostrado no display
        change_number(vry_value, vrx_value, &current_number, vrx_calibration, vry_calibration);
        sprintf(current_number_str, "%d", current_number); // Converte o número para uma string
        
        int len = strlen(current_number_str); // Calcula o comprimento da string do número
        int number_pos_x = (WIDTH - len * 6) / 2; // Posição X do número no display de acordo com o comprimento da string
        int len_expression = strlen(expression); // Calcula o comprimento da string da expressão
        int expression_pos_x = (100 - len_expression * 6) / 2;  // Posição X da expressão no display de acordo com o comprimento da string
        
        ssd1306_rect(&ssd, 3, 3, 122, 60, 1, 0);// Desenha um retângulo no display
        ssd1306_draw_string(&ssd, expression, expression_pos_x, EXPRESSION_POS_Y);
        ssd1306_draw_string(&ssd, current_number_str, number_pos_x, NUMBER_POS_Y);
        ssd1306_send_data(&ssd);

        if(press_button_a)// Verifica se o botão A foi pressionado 
        {
          new_expression = check_answer(current_number, result, valor_led, pio, sm);// Verifica a resposta do usuário
          press_button_a = false; // Reseta a flag do botão A
        }
        ssd1306_fill(&ssd, 0);
      }
    }

    sleep_ms(100);
  };  
}

// Função para tocar uma nota musical
// A função recebe o pino do buzzer, a frequência da nota e a duração
void play_note(int buzzer, int frequency, int duration) {
  if (frequency == 0)
  {
    sleep_ms(duration);  // Pausa se a frequência for 0
    return;
  }

  int delay = 1000000 / frequency / 2; // Meio ciclo da frequência
  int cycles = (frequency * duration) / 1000; // Número de ciclos para a duração

  for (int i = 0; i < cycles; i++) {
      gpio_put(buzzer, 1); // Liga o buzzer
      sleep_us(delay); // Aguarda o tempo do ciclo
      gpio_put(buzzer, 0); // Desliga o buzzer
      sleep_us(delay);// Aguarda o tempo do ciclo
  }
}

// Função para tocar uma melodia
// A função recebe um ponteiro para a estrutura de notas e o pino do buzzer
void play_melody(Note *melody, int buzzer) {
  for (int i = 0; melody[i].frequency != 0 || melody[i].duration != 0; i++)
  {
    play_note(buzzer, melody[i].frequency, melody[i].duration); // Toca a nota de acordo com a frequência e duração
    sleep_ms(100);  // Pequena pausa entre as notas
  }
}
// Função de tratamento de interrupção do GPIO
static void gpio_irq_handler(uint gpio, uint32_t events)
{
  // Obter o tempo atual para o debounce
  absolute_time_t current_time = get_absolute_time();

  if (absolute_time_diff_us(last_interrupt_time, current_time) < DEBOUNCE_TIME_MS * 1000)
  {
    return; // Ignora a interrupção se estiver dentro do tempo de debounce
  }
  else
  {
    last_interrupt_time = current_time;
  }

    if(gpio == PIN_BUTTON_B)
    {
      is_on = !is_on; // Liga/desliga o sistema
    }
    else if (gpio == PIN_BUTTON_A)
    {
      if(is_on)
      {
        press_button_a = true; // Flag para o botão A
      }
    }
    else if (gpio == SW_PIN)
    {
      // Ativar o BOOTSEL
      printf("BOOTSEL ativado.\n");
      reset_usb_boot(0, 0);
    }
}

// Função para criar uma expressão matemática aleatória
// A função recebe um ponteiro para uma string e um ponteiro para um inteiro
void create_expression(char *expression, int *result)
{
  // Gera dois números aleatórios entre 1 e 100
  int op = rand() % 2; // Gera um número aleatório entre 0 e 1 para a representação da operação
  int num1 = rand() % 100 + 1;
  int num2 = rand() % 100 + 1;
  if (op == 0)
  {
    *result = num1 + num2;
    sprintf(expression, "%d + %d = ?", num1, num2); // Cria a expressão matemática pra soma
  }
  else
  {
    *result = num1 - num2;
    sprintf(expression, "%d - %d = ?", num1, num2); // Cria a expressão matemática pra subtração
  }
  printf(expression);
  printf("\n");
}

// Função para alterar o número selecionado, de acordo com o movimento do joystick
// A função recebe os valores dos eixos X e Y do joystick, o número atual, os valores de calibração dos eixos X e Y
void change_number(uint16_t vry_value, uint16_t vrx_value, int *number, uint16_t vrx_calibration, uint16_t vry_calibration)
{
  // Define os limites de sensibilidade do joystick
  const uint16_t threshold = 1000;

  // Verifica o movimento no eixo X (VRX)
  if (vrx_value > vrx_calibration + threshold && !vrx_moved)
  {
    // Movimento para a direita
    *number += 1;
    vrx_moved = true;
  }
  else if (vrx_value < vrx_calibration - threshold && !vrx_moved)
  {
    // Movimento para a esquerda
    *number -= 1;
    vrx_moved = true;
  }
  else if (vrx_value >= vrx_calibration - threshold && vrx_value <= vrx_calibration + threshold)
  {
    // Joystick voltou ao centro
    vrx_moved = false;
  }

  // Verifica o movimento no eixo Y (VRY)
  if (vry_value > vry_calibration + threshold && !vry_moved)
  {
    // Movimento para cima
    *number += 10;
    vry_moved = true;
  }
  else if (vry_value < vry_calibration - threshold && !vry_moved)
  {
    // Movimento para baixo
    *number -= 10;
    vry_moved = true;
  }
  else if (vry_value >= vry_calibration - threshold && vry_value <= vry_calibration + threshold)
  {
    // Joystick voltou ao centro
    vry_moved = false;
  }
  // Atualize os valores anteriores do joystick
  prev_vrx_value = vrx_value;
  prev_vry_value = vry_value;
}

// Função para selecionar o modo de operação
// A função recebe o valor do eixo X do joystick, um ponteiro para um booleano e o valor de calibração do eixo X
void select_mode( uint16_t vrx_value, bool *index, uint16_t vrx_calibration)
{
  bool current_index = *index;
  // Define os limites de sensibilidade do joystick
  const uint16_t threshold = 1000;

  // Verifica o movimento no eixo Y (VRY)
  if (vrx_value > vrx_calibration + threshold && !vry_moved)
  {
    *index = true;
    vry_moved = true;
  }
  else if (vrx_value < vrx_calibration - threshold && !vry_moved)
  {
    *index = false;
    vry_moved = true;
  }
  else if (vrx_value >= vrx_calibration - threshold && vrx_value <= vrx_calibration + threshold)
  {
    // Joystick voltou ao centro
    vry_moved = false;
  }

  if (current_index != *index)
  {
    play_note(BUZZER_PIN, 500,50);
  }
  // Atualiza os valores anteriores do joystick
  prev_vrx_value = vrx_value;
}

// Função para converter um valor RGB para um valor de 32 bits
// A função recebe os valores de vermelho, verde e azul do LED
uint32_t matrix_rgb(double r, double g, double b)
{
  unsigned char R, G, B;
  R = r * 255;
  G = g * 255;
  B = b * 255;
  return (G << 24) | (R << 16) | (B << 8);
}

//rotina para acionar a matrix de leds
void pio_drawn(double *desenho, uint32_t valor_led, PIO pio, uint sm, double r, double g, double b)
{
  for (int16_t i = 0; i < NUM_PIXELS; i++)
  {
    uint32_t valor_led = matrix_rgb(r * desenho[24 - i], g * desenho[24 - i], b * desenho[24 - i]);
    pio_sm_put_blocking(pio, sm, valor_led);
  }
}

// Função para indicar uma resposta correta
// A função recebe o valor do LED, o pino do PIO e o estado da máquina
void correct_answer( uint32_t valor_led, PIO pio, uint sm)
{
  gpio_put(LED_PIN_GREEN, true); 
  pio_drawn(matrix_correct, valor_led, pio, sm, 0, 1, 0); // Desenha o padrão de LEDs para resposta correta na cor verde
  play_melody(melody_correct, BUZZER_PIN);// Toca a melodia de resposta correta
  pio_drawn(NULL, valor_led, pio, sm, 0, 0, 0);// Desliga todos os LEDs

  gpio_put(LED_PIN_GREEN, false);
}

// Função para indicar uma resposta incorreta
// A função recebe o valor do LED, o pino do PIO e a máquina de estado
void incorrect_answer(uint32_t valor_led, PIO pio, uint sm)
{
  gpio_put(LED_PIN_RED, true);
  pio_drawn(matrix_incorrect, valor_led, pio, sm, 1, 0, 0);// Desenha o padrão de LEDs para resposta incorreta na cor vermelha
  play_melody(melody_incorrect, BUZZER_PIN);// Toca a melodia de resposta incorreta
  pio_drawn(NULL, valor_led, pio, sm, 0, 0, 0);// Desliga todos os LEDs

  gpio_put(LED_PIN_RED, false);
}

// Função para verificar a resposta do usuário
// A função recebe o número atual, o resultado da expressão, o valor do LED, o pino do PIO e a máquina de estado
bool check_answer(int current_number, int result, uint32_t valor_led, PIO pio, uint sm)
{
  if (current_number == result) // Verifica se a resposta está correta
    {
      printf("Resposta correta!\n");
      correct_answer(valor_led, pio, sm);
      return true;
    }
    else
    {
      printf("Resposta incorreta!\n");
      incorrect_answer(valor_led, pio, sm);
      return false;
    }
}
