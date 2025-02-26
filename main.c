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

// Definições de pinos do joystick, botões, buzzer e LEDs
#define VRY_PIN 26  
#define VRX_PIN 27
#define SW_PIN 22

#define LED_PIN_RED 13
#define LED_PIN_GREEN 11

#define PIN_BUTTON_A 5
#define PIN_BUTTON_B 6
#define BUZZER_PIN 21
#define OUT_PIN 7

#define NUM_PIXELS 25

#define DEBOUNCE_TIME_MS 300 // Tempo de debounce em ms

#define EXPRESSION_POS_Y 10
#define NUMBER_POS_Y 30

absolute_time_t last_interrupt_time = {0};
ssd1306_t ssd; 

bool press_button_a = false;
bool is_on = false;
bool is_serial_mode = false;
uint16_t prev_vrx_value = 0;
uint16_t prev_vry_value = 0;
bool vrx_moved = false;
bool vry_moved = false;

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
void change_index( uint16_t vrx_value, bool *index, uint16_t vrx_calibration);

int main() { 
  
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
  pio_drawn(NULL, valor_led, pio, sm, 0, 0, 0);


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


  adc_select_input(2); // Use um pino ADC não conectado
  uint16_t seed = adc_read();
  srand(seed);

  char expression[20];
  int result;
  int current_number = 0;
  char current_number_str[10];

  int selection_index = 0;
  while(true)
  {
    adc_select_input(1);
    vrx_value = adc_read();
    ssd1306_fill(&ssd, 0);
    ssd1306_rect(&ssd, 3, 3, 122, 60, 1, 0);
    ssd1306_draw_string(&ssd, "Para iniciar", 10, 10);
    ssd1306_draw_string(&ssd, "Selecione o", 10, 20);
    ssd1306_draw_string(&ssd, "Modo", 10, 30);

    // Desenhar duas áreas retangulares
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
    change_index(vrx_value, &is_serial_mode, vrx_calibration);
    while (is_on)
    {
      bool new_expression = false;
      create_expression(expression, &result);
      current_number = 0;
      ssd1306_fill(&ssd, 0);
      while (!new_expression && is_on && is_serial_mode)
      {
        int len = strlen(current_number_str);
        int number_pos_x = (WIDTH - len * 6) / 2; // 6 é a largura de um caractere
        int len_expression = strlen(expression);
        int expression_pos_x = (100 - len_expression * 6) / 2; // 6 é a largura de um caractere
        ssd1306_rect(&ssd, 3, 3, 122, 60, 1, 0);
        ssd1306_draw_string(&ssd, expression, expression_pos_x, EXPRESSION_POS_Y);
        ssd1306_send_data(&ssd);
        
        char input[30]; 
        printf("Digite um número: ");
        scanf("%s", input);
        if(strcmp(input, "exit") == 0)
        {
          is_on = false;
          break;
        }
        current_number  = atoi(input);
        sprintf(current_number_str, "%d", current_number);
        ssd1306_draw_string(&ssd, current_number_str, number_pos_x, NUMBER_POS_Y);
        ssd1306_send_data(&ssd);
        printf("Resposta: %d\n", current_number);

        //TODO melhorar feedback no serial mode


        new_expression = check_answer(current_number, result, valor_led, pio, sm);
        ssd1306_fill(&ssd, 0);
      }
      while (!new_expression && is_on && !is_serial_mode)
      {
        ssd1306_fill(&ssd, 0);
        adc_select_input(0);
        vry_value = adc_read();
        adc_select_input(1);
        vrx_value = adc_read();
        
        change_number(vry_value, vrx_value, &current_number, vrx_calibration, vry_calibration);
        sprintf(current_number_str, "%d", current_number);
        
        int len = strlen(current_number_str);
        int number_pos_x = (WIDTH - len * 6) / 2; // 6 é a largura de um caractere
        
        int len_expression = strlen(expression);
        int expression_pos_x = (100 - len_expression * 6) / 2; // 6 é a largura de um caractere
        
        ssd1306_rect(&ssd, 3, 3, 122, 60, 1, 0);
        ssd1306_draw_string(&ssd, expression, expression_pos_x, EXPRESSION_POS_Y);
        ssd1306_draw_string(&ssd, current_number_str, number_pos_x, NUMBER_POS_Y);
        ssd1306_send_data(&ssd);

        if(press_button_a)
        {
          new_expression = check_answer(current_number, result, valor_led, pio, sm);
          press_button_a = false;
        }
        ssd1306_fill(&ssd, 0);

      }
    }

    sleep_ms(100);
  };  
}

void play_note(int buzzer, int frequency, int duration) {
  if (frequency == 0) {
      sleep_ms(duration);  // Pausa (silêncio)
      return;
  }

  int delay = 1000000 / frequency / 2; // Meio ciclo da frequência
  int cycles = (frequency * duration) / 1000;

  for (int i = 0; i < cycles; i++) {
      gpio_put(buzzer, 1);
      sleep_us(delay);
      gpio_put(buzzer, 0);
      sleep_us(delay);
  }
}

void play_melody(Note *melody, int buzzer) {
  for (int i = 0; melody[i].frequency != 0 || melody[i].duration != 0; i++)
  {
    play_note(buzzer, melody[i].frequency, melody[i].duration);
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
      is_on = !is_on;
    }
    else if (gpio == PIN_BUTTON_A)
    {
      if(is_on)
      {
        press_button_a = true;
      }
    }
    else if (gpio == SW_PIN)
    {
              // Ativar o BOOTSEL
      printf("BOOTSEL ativado.\n");
      reset_usb_boot(0, 0);
    }
}

void create_expression(char *expression, int *result)
{
  // Gera dois números aleatórios entre 1 e 100
  int op = rand() % 2;
  int num1 = rand() % 100 + 1;
  int num2 = rand() % 100 + 1;
  if (op == 0)
  {
    *result = num1 + num2;
    sprintf(expression, "%d + %d = ?", num1, num2);
  }
  else
  {
    *result = num1 - num2;
    sprintf(expression, "%d - %d = ?", num1, num2);
  }
  printf("num1: %d, op: %d,  num2: %d, result: %d\n", num1, op, num2, *result);
}

void change_number(uint16_t vry_value, uint16_t vrx_value, int *number, uint16_t vrx_calibration, uint16_t vry_calibration)
{
    // Defina os limites de sensibilidade do joystick
    const uint16_t threshold = 1000;

    // Verifique o movimento no eixo X (VRX)
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

    // Verifique o movimento no eixo Y (VRY)
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

void change_index( uint16_t vrx_value, bool *index, uint16_t vrx_calibration)
{
  // Defina os limites de sensibilidade do joystick
  const uint16_t threshold = 1000;

  // Verifique o movimento no eixo Y (VRY)
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

  //TODO Sinal sonoro ao modficar o index e ao selecionar o item

  // Atualize os valores anteriores do joystick
  prev_vrx_value = vrx_value;
}

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

void correct_answer( uint32_t valor_led, PIO pio, uint sm)
{
  gpio_put(LED_PIN_GREEN, true);
  pio_drawn(matrix_correct, valor_led, pio, sm, 0, 1, 0);
  play_melody(melody_correct, BUZZER_PIN);
  pio_drawn(matrix_correct, valor_led, pio, sm, 0, 0, 0);

  gpio_put(LED_PIN_GREEN, false);
}

void incorrect_answer(uint32_t valor_led, PIO pio, uint sm)
{
  gpio_put(LED_PIN_RED, true);
  pio_drawn(matrix_incorrect, valor_led, pio, sm, 1, 0, 0);
  play_melody(melody_incorrect, BUZZER_PIN);
  pio_drawn(matrix_incorrect, valor_led, pio, sm, 0, 0, 0);

  gpio_put(LED_PIN_RED, false);
}

bool check_answer(int current_number, int result, uint32_t valor_led, PIO pio, uint sm)
{
  if (current_number == result)
    {
      correct_answer(valor_led, pio, sm);
      return true;
    }
    else
    {
      incorrect_answer(valor_led, pio, sm);
      return false;
    }
}
