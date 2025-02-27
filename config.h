// Constantes para as notas musicais
#define NOTE_F4   349
#define NOTE_A4   440
#define NOTE_A4S  466
#define NOTE_B4   494
#define NOTE_C5   523

// Duração das notas (1 segundo = 1000 ms)
#define WHOLE     1000
#define HALF      500
#define QUARTER   250
#define EIGHTH    125

// Estrutura para armazenar uma nota musical
typedef struct {
    int frequency; // Frequência da nota
    int duration; // Duração da nota
} Note;

// Melodia para resposta correta
Note melody_correct[] = {
    {0, EIGHTH},
    {NOTE_F4, EIGHTH}, {NOTE_F4, EIGHTH}, {NOTE_A4, EIGHTH},
    {NOTE_C5, QUARTER}, {NOTE_A4, EIGHTH}, {NOTE_C5, HALF},
    {0, 0}
};

// Melodia para resposta incorreta
Note melody_incorrect[] = {
    {0, EIGHTH},
    {NOTE_C5, QUARTER}, {NOTE_B4, QUARTER},
    {NOTE_A4S, QUARTER}, {NOTE_A4, HALF},
    {0, 0}
};

// Matriz de LEDs para resposta correta
double matrix_correct[25] =
    {0.0, 0.1, 0.1, 0.1, 0.0,
     0.1, 0.0, 0.0, 0.0, 0.1,
     0.1, 0.0, 0.0, 0.0, 0.1,
     0.1, 0.0, 0.0, 0.0, 0.1,
     0.0, 0.1, 0.1, 0.1, 0.0};

// Matriz de LEDs para resposta incorreta
double matrix_incorrect[25] =
    {0.1, 0.0, 0.0, 0.0, 0.1,
     0.0, 0.1, 0.0, 0.1, 0.0,
     0.0, 0.0, 0.1, 0.0, 0.0,
     0.0, 0.1, 0.0, 0.1, 0.0,
     0.1, 0.0, 0.0, 0.0, 0.1};