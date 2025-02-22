#include <stdio.h>            
#include <stdlib.h>
#include <time.h>
#include "pico/stdlib.h"      
#include "hardware/adc.h"     
#include "hardware/pwm.h"     
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
#define I2C_SDA 14
#define I2C_SCL 15
#define endereco 0x3C

// Definições de pinos do joystick, botões e LEDs
#define VRY_PIN 26  
#define VRX_PIN 27
#define SW_PIN 22
#define LED_PIN_RED 13
#define LED_PIN_GREEN 11
#define LED_PIN_BLUE 12
#define PIN_BUTTON_A 5
#define PIN_BUTTON_B 6
#define BUZZER_PIN 21
#define OUT_PIN 7

#define NUM_PIXELS 25

#define DEBOUNCE_TIME_MS 300 // Tempo de debounce em ms

absolute_time_t last_interrupt_time = {0};
ssd1306_t ssd; 

bool playsong1 = false;
bool playsong2 = false;

// Inicializa o PWM para um pino GPIO específico
uint pwm_init_gpio(uint gpio, uint wrap) {
    gpio_set_function(gpio, GPIO_FUNC_PWM);

    uint slice_num = pwm_gpio_to_slice_num(gpio);
    pwm_set_wrap(slice_num, wrap);
    
    pwm_set_enabled(slice_num, true);  
    return slice_num;  
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

  // Ativa ou desativa a funcionalidade do LED com PWM quando o botão A é pressionado
    if(gpio == PIN_BUTTON_B)
    {
        // Ativar o BOOTSEL
      printf("BOOTSEL ativado.\n");
      reset_usb_boot(0, 0);
    }
    else if (gpio == PIN_BUTTON_A)
    {
      playsong1 = true;
    }
    else if (gpio == SW_PIN)
    {
      playsong2 = true;
    }
}

void create_expression(char *expression, int *result)
{
  // Inicializa o gerador de números aleatórios
  srand(time(NULL));

  // Gera dois números aleatórios entre 1 e 100
  int op = rand() % 2;
  while (true)
  {
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
    if (*result >= 0)
    {
      printf("num1: %d, op: %d,  num2: %d, result: %d\n", num1, op, num2, *result);
      break;
    }
  }
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
  gpio_init(LED_PIN_BLUE);
  gpio_init(LED_PIN_RED);
  gpio_init(BUZZER_PIN);

  // Configura os pinos dos botões e LEDs
  gpio_set_dir(PIN_BUTTON_A, GPIO_IN);
  gpio_set_dir(PIN_BUTTON_B, GPIO_IN);
  gpio_set_dir(SW_PIN, GPIO_IN);
  gpio_set_dir(BUZZER_PIN, GPIO_OUT);
  gpio_set_dir(LED_PIN_GREEN, GPIO_OUT);
  gpio_set_dir(LED_PIN_BLUE, GPIO_OUT);
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

  char expression[20];
  int result;
  while (true)
  {
    create_expression(expression, &result);
    ssd1306_fill(&ssd, 0);
    ssd1306_draw_string(&ssd, expression, 5, 5);
    ssd1306_send_data(&ssd);
    
    while (true)
    {
      char input[30]; 
      printf("Digite um número: ");
      scanf("%s", input);
      int answer  = atoi(input);
      printf("Resposta: %d\n", answer);
  
      if (answer == result)
      {
        correct_answer(valor_led, pio, sm);
        break;
      }
      else
      {
        incorrect_answer(valor_led, pio, sm);
      }
    }
    
    sleep_ms(100);
  };  
}
