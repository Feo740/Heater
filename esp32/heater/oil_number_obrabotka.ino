void oil_number_obrabotka (){

byte f=0; // флаг начала элемента
byte j=0; // счетчик для массива символа
byte l=0; // счетчик для конечного массива адресов
char simbol[2];
byte st_razriad;
byte ml_razriad;

for (int i=0; i<2; i++){
  simbol[i] = 0;
}

for (int i=0; i<40; i++){
  // проверяем в строке конец символа
  if (oil_buffer[i] == ','){
    f=0; //флаг - конец символа
    j=0; // счетчик для массива символа
    switch(simbol[0]){
      case '0':
      st_razriad=0;
      break;
      case '1':
      st_razriad=1;
      break;
      case '2':
      st_razriad=2;
      break;
      case '3':
      st_razriad=3;
      break;
      case '4':
      st_razriad=4;
      break;
      case '5':
      st_razriad=5;
      break;
      case '6':
      st_razriad=6;
      break;
      case '7':
      st_razriad=7;
      break;
      case '8':
      st_razriad=8;
      break;
      case '9':
      st_razriad=9;
      break;
      case 'A':
      st_razriad=10;
      break;
      case 'B':
      st_razriad=11;
      break;
      case 'C':
      st_razriad=12;
      break;
      case 'D':
      st_razriad=13;
      break;
      case 'E':
      st_razriad=14;
      break;
      case 'F':
      st_razriad=15;
      break;
        }

    switch(simbol[1]){
          case '0':
          ml_razriad=0;
          break;
          case '1':
          ml_razriad=1;
          break;
          case '2':
          ml_razriad=2;
          break;
          case '3':
          ml_razriad=3;
          break;
          case '4':
          ml_razriad=4;
          break;
          case '5':
          ml_razriad=5;
          break;
          case '6':
          ml_razriad=6;
          break;
          case '7':
          ml_razriad=7;
          break;
          case '8':
          ml_razriad=8;
          break;
          case '9':
          ml_razriad=9;
          break;
          case 'A':
          ml_razriad=10;
          break;
          case 'B':
          ml_razriad=11;
          break;
          case 'C':
          ml_razriad=12;
          break;
          case 'D':
          ml_razriad=13;
          break;
          case 'E':
          ml_razriad=14;
          break;
          case 'F':
          ml_razriad=15;
          break;
            }

    if (simbol[1] == 0){
      t[l] = st_razriad;
      l++;
    }
    else{
      t[l] = st_razriad*16+ml_razriad;
      l++;
        }


  }

  // если мы уже читам сам символ из строки
  if (f ==1){
    simbol[j] = oil_buffer[i];
    j++;
  }
  // проверяем в строке начало нового символа
  if (oil_buffer[i] == 'x'){
    f=1; //флаг начала символа
    j=0; // счетчик для массива символа
    for (int i=0; i<2; i++){
          simbol[i] = 0;
        }
  }
}
}
