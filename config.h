#define NOTE_C3   131
#define NOTE_G3   196
#define NOTE_A3   220
#define NOTE_A3S  233
#define NOTE_B3   247
#define NOTE_C4   262
#define NOTE_D4   294
#define NOTE_E4   330
#define NOTE_F4   349
#define NOTE_G4   392
#define NOTE_A4   440
#define NOTE_A4S  466
#define NOTE_B4   494
#define NOTE_C5   523

// Duração das notas (1 segundo = 1000 ms)
#define WHOLE     1000
#define HALF      500
#define QUARTER   250
#define EIGHTH    125

typedef struct {
    int frequency;
    int duration;
} Note;

Note melody_correct[] = {
    {0, EIGHTH},
    {NOTE_F4, EIGHTH}, {NOTE_F4, EIGHTH}, {NOTE_A4, EIGHTH}, {NOTE_C5, QUARTER},
    {NOTE_A4, EIGHTH}, {NOTE_C5, HALF},
    {0, 0}
};

Note melody_incorrect[] = {
    {0, EIGHTH},
    {NOTE_C5, QUARTER}, {NOTE_B4, QUARTER},
    {NOTE_A4S, QUARTER}, {NOTE_A4, HALF},
    {0, 0}
};

double matrix_correct[25] =
    {0.0, 0.1, 0.1, 0.1, 0.0,
     0.1, 0.0, 0.0, 0.0, 0.1,
     0.1, 0.0, 0.0, 0.0, 0.1,
     0.1, 0.0, 0.0, 0.0, 0.1,
     0.0, 0.1, 0.1, 0.1, 0.0};

double matrix_incorrect[25] =
    {0.1, 0.0, 0.0, 0.0, 0.1,
     0.0, 0.1, 0.0, 0.1, 0.0,
     0.0, 0.0, 0.1, 0.0, 0.0,
     0.0, 0.1, 0.0, 0.1, 0.0,
     0.1, 0.0, 0.0, 0.0, 0.1};