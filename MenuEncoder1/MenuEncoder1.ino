/*
 * Implementação de menu no display OLED I2C 128x64
 * Controlado por um encoder com chave
 * 
 * (C) 2020, Daniel Quadros
 */

#include <Wire.h>
#include <TimerOne.h>

// Estrutura para controlar um menu
typedef struct {
  char *titulo;
  int nItens;
  char **opcoes;
} MENU;

// Fila da ações lidas do Rotary Encoder
typedef enum { NADA, UP, DOWN, ENTER } AcaoEnc;
const byte NBITS_FILA = 4;
const byte tamFilaEnc = 1 << NBITS_FILA;
volatile AcaoEnc filaEnc[tamFilaEnc];
volatile byte poeEnc = 0;
volatile byte tiraEnc = 0;

// Coloca uma ação na fila
// Só chamar na interrupção
void inline poeFilaEnc(AcaoEnc acao) {
  byte temp = (poeEnc + 1) & (tamFilaEnc-1);
  if (temp != tiraEnc) {
    filaEnc[poeEnc] = acao;
    poeEnc = temp;
  }
}

// Retira uma ação da fila
// Não chamar na interrupção
AcaoEnc tiraFilaEnc() {
  AcaoEnc ret = NADA;
  noInterrupts();
  if (poeEnc != tiraEnc) {
    ret = filaEnc[tiraEnc];
    byte temp = (tiraEnc + 1) & (tamFilaEnc-1);
    tiraEnc = temp;
  }
  interrupts();
  return ret;
}

// Menus
char *opcPrincipal[] = {
  "Menu Secundario",
  "Seleciona Valor",
  "Sobre"
};
MENU principal = {
  "Principal",
  sizeof(opcPrincipal)/sizeof(char *),
  opcPrincipal
};

char *opcSecundario[] = {
  "Opcao 1",
  "Opcao 2",
  "Opcao 3",
  "Opcao 4",
  "Opcao 5",
  "Volta"
};
MENU secundario = {
  "Secundario",
  sizeof(opcSecundario)/sizeof(char *),
  opcSecundario
};

// Iniciação
void setup() {
  pinMode(13, OUTPUT);
  digitalWrite(13, LOW);
  pinMode(12, INPUT);
  pinMode(A0, INPUT);
  pinMode(A1, INPUT);
  Wire.begin();
  Timer1.initialize(1000);  // 1 milisegundo
  Timer1.attachInterrupt(trataEncoder);
  Display_init();
}

// Programa Principal
void loop() {
  switch (leMenu (&principal)) {
    case 0:
      menuSecundario();
      break;
    case 1:
      selValor();
      break;
    case 2:
      Display_clear();
      Display_print (2, 0, "Teste do", 0);
      Display_print (3, 0, "Rotary", 0);
      Display_print (4, 0, "Encoder", 0);
      delay(2000);
      break;
  }
}

// Menu Secundario
void menuSecundario() {
  while (true) {
    int sel = leMenu(&secundario);
    if (sel == 5) {
      return;
    }
    Display_clear();
    Display_print (2, 0, "Opcao:", 0);
    Display_write (2, 7, sel+0x31);
    delay (2000);
  }
}

// Teste do encoder para selecionar um valor de 0 a 99
void selValor() {
  byte valor = 10;
  AcaoEnc acao = UP;
  Display_clear();
  Display_print (2, 0, "Valor:", 0);
  while (true) {
    if (acao != NADA) {
      Display_write (2, 7, 0x30+(valor/10));
      Display_write (2, 8, 0x30+(valor%10));
    }
    acao = tiraFilaEnc();
    switch (acao) {
      case ENTER:
        return;
      case DOWN:
        if (valor < 99) {
          valor++;
        }
        break;
      case UP:
        if (valor > 0) {
          valor--;
        }
        break;
    }
  }
}

/**
 * Rotina chamada periodicamente pelo Timer2
 * Precisa ser rápida para não interferir nas
 * interrupções de I2C da biblioteca Wire
 */
void trataEncoder() {
  static byte swAnt = 1;
  static byte clkAnt = 1;
  static byte swDb = 1;
  static byte clkDb = 1;

  // Vamos ler direto do hw para ganhar tempo
  byte sw = (PINB & 0x10) != 0;  // D12 = PB4
  byte dt = (PINC & 0x01) != 0;  // A0  = PC0
  byte clk = (PINC & 0x02) != 0; // A1  = PC1

  // Debounce da chave
  if (sw == swAnt) {
    if (sw != swDb) {
      if (sw) {
        poeFilaEnc(ENTER);
      }
      swDb = sw;
    }
  } else {
    swAnt = sw;
  }

  // Debounce do encoder
  if (clkAnt == clk) {
    if (clk != clkDb) {
      if (clk) {
        poeFilaEnc(dt == clk? UP : DOWN);
      }
      clkDb = clk;
    }
  } else {
    clkAnt = clk;
  }
}

/**
 * Rotina para apresentar o menu e ler uma opção
 */
int leMenu (MENU *menu) {
  int topo = 0;
  int old_topo = -1;
  int sel = 0;
  int n = menu->nItens;
  if (n > 5) {
    n = 5;
  }
  int ant = -1;
  Display_clear();
  Display_print (0, 0, menu->titulo, 0);
  Display_print (1, 0, "--------------------", 0);
  while (true) {
      if (sel != ant) {
        // Rola se necessário
        if (sel < topo) {
          topo = sel;
        }
        if (sel > (topo+4)) {
          topo = sel - 4;
        }
        // Redesenha o menu
        if (topo != old_topo) {
          for (int i = 0; i < n; i++) {
            int opc = topo+i;
            Display_clearline (2+i);
            if (opc == sel) {
              Display_print (2+i, 0, menu->opcoes[opc], 0xFF);
            } else {
              Display_print (2+i, 0, menu->opcoes[opc], 0);
            }
          }
          old_topo = topo;
        } else {
          Display_print (2+ant-topo, 0, menu->opcoes[ant], 0);
          Display_print (2+sel-topo, 0, menu->opcoes[sel], 0xFF);
        }
        ant = sel;
      }
      switch (tiraFilaEnc()) {
        case ENTER:
          return sel;
        case UP:
          if (sel > 0) {
            sel--;
          }
          break;
       case DOWN:
          if (sel < (menu->nItens-1)) {
            sel++;
          }
      }
  }
}
