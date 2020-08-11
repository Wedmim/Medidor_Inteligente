//  Placa: DOIT ESP32 DEVKIT V1
//  Upload Speed: 11520

#include "ThingSpeak.h" //ref: http://dev.toppers.jp/trac_user/contrib/browser/rtos_arduino/trunk/arduino_lib/libraries/thingspeak-arduino/src/ThingSpeak.h?rev=189
#include <WiFi.h>
#include "time.h" //ref: https://randomnerdtutorials.com/esp32-date-time-ntp-client-server-arduino/
#include <Arduino_JSON.h>
#include <HTTPClient.h>

String html, header;
String meses [12] = {"JAN", "FEV", "MAR", "ABR", "MAY", "JUN", "JUL", "AGO", "SET", "OUT", "NOV", "DEZ"};

struct tm timeinfo; //struct que contem as informações de tempo. ref: http://www.cplusplus.com/reference/ctime/tm/
int maxAmount = 7, //  Quantidad maxima de pontos que podem ser exibidos no grafico, começando em 7 [dias]
    hdmAtual = timeinfo.tm_mday;   //  "Hora", "Dia" ou "Mês"  atual

//configurações de tempo:
const char* ntpServer = "pool.ntp.org"; //de onde coletá-las
const long  gmtOffset_sec = -10800;     //fuso horario
const int   daylightOffset_sec = 0;     //horario de verao

float  consumo_diario[31], //31 dias
       consumo_mensal[12], //12 meses
       consumo_hora[24],   //24 horas
       TENSAO = 0,
       CORRENTE = 0,
       FP = 0,
       precokWh = 0,
       dinheiroPorDia = 0,
       dinheiroPorMes = 0,
       meta = 0;

//variaveis relacionadas ao API:
//String jsonBuffer;
//int TariffFlag_number = 0;
//String TariffFlag_name = "Verde";

//variaveis relacionadas ao ThingSpeak:
unsigned long myChannelNumber = 1076260;
const char * myWriteAPIKey = "UICL5S86IO3X0NQF";
String myStatus = "";

unsigned long counterChannelNumber = 1076260;
const char * myCounterReadAPIKey = "HB0Y6SB0W3ZHHX9P";

WiFiServer server(80);
WiFiClient client;

void setup() {
  Serial.begin(115200);

  //conectar a rede wifi:
  Serial.print("Conectando-se a ");
  Serial.println("PAIXAOLAYBER-4944");
  WiFi.begin("PAIXAOLAYBER-4944", "paixao1098");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi conectada.");
  Serial.println("Endereço de IP: ");
  Serial.println(WiFi.localIP());
  Serial.println(WiFi.getHostname());

  server.begin();

  //configurações de tempo:
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  printLocalTime(); //printar informações de tempo no monitor serial

  //configurações de ThingSpeak:
  WiFi.mode(WIFI_STA);
  ThingSpeak.begin(client);  // Initialize ThingSpeak

  for (int i = 0; i < 31; i++) {
    if (i < 12) {
      consumo_mensal[i] = 0;
    }
    if (i < 24) {
      consumo_hora[i] = 0;
    }
    consumo_diario[i] = 0;
  }
}

void loop() {
  unsigned long timerDelay3 = 15000;
  unsigned long lastTime3 = 0;

  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
  }

  //APIdoSetorEletrico();   //GET JSON. ref: https://randomnerdtutorials.com/esp32-http-get-open-weather-map-thingspeak-arduino/
  //TariffInfo();
  //ThingSpeakPost();

  if ((millis() - lastTime3) > timerDelay3) {   //Leitura dos dados no ThingSpeak a cada 15 segundos

    Serial.println("---------------------------------");
    Serial.println("Corrente [A]: ");
    CORRENTE = ThingSpeakRead(1);
    Serial.println("Tensao [V]: ");
    TENSAO = ThingSpeakRead(2);
    Serial.println("Dinheiro consumido por DIA [R$]: ");
    dinheiroPorDia = ThingSpeakRead(4);
    Serial.println("Dinheiro consumido no MES [R$]: ");
    dinheiroPorMes = ThingSpeakRead(3);
    Serial.println("FP: ");
    FP = ThingSpeakRead(6);
    attConsumo(5);
    Serial.println("Preco do kWh [R$]: ");
    precokWh = ThingSpeakRead(8);
    Serial.println("---------------------------------");
    lastTime3 = millis();
  }
  http();
}

void attConsumo(int fieldTS) {
  int i;
  Serial.println("Potencia Consumida na HORA [kWh]: ");
  consumo_hora[timeinfo.tm_hour] = ThingSpeakRead(fieldTS);

  if (timeinfo.tm_hour == 23 && timeinfo.tm_min == 59) { //Quando o dia acabar
    consumo_diario[timeinfo.tm_mday - 1] = consumo_hora[timeinfo.tm_hour]; //Armazenar o maior valor consumido do dia

    if (timeinfo.tm_mday == 28) { // Mes com 28 dias (Fevereiro)
      if (timeinfo.tm_mon == 1) {
        for (i = 0; i < 28; i++) {  //Somatorio do consumo em todos os dias do mes
          consumo_mensal[1] += consumo_diario[i];
          consumo_diario[i] = 0;
        }
      }
    }
    else if (timeinfo.tm_mday == 30) { // Meses com 30 dias (Abril, Junho, Setembro, Novembro)
      if (timeinfo.tm_mon == 10 || timeinfo.tm_mon == 8 || timeinfo.tm_mon == 5 || timeinfo.tm_mon == 3) {
        for (i = 0; i < 30; i++) {  //Somatorio do consumo em todos os dias do mes
          consumo_mensal[timeinfo.tm_mon] += consumo_diario[i];
          consumo_diario[i] = 0;
        }
      }
    }
    else if (timeinfo.tm_mday == 31) { // Meses com 31 dias (Janeiro, Março, Maio, Julho, Agosto, Outubro, Dezembro)
      for (i = 0; i < 31; i++) {  //Somatorio do consumo em todos os dias do mes
        consumo_mensal[timeinfo.tm_mon] += consumo_diario[i];
        consumo_diario[i] = 0;
      }
    }
  }

  Serial.println("Potencia Consumida no DIA [kWh]: ");
  Serial.println(consumo_diario[timeinfo.tm_mday - 1], 5);
  Serial.println("Potencia Consumida no MES [kWh]: ");
  Serial.println(consumo_mensal[timeinfo.tm_mon], 5);
}

float ThingSpeakRead(unsigned int counterFieldNumber) {
  // Read in field 1 of the private channel which is a counter
  float count = ThingSpeak.readFloatField(counterChannelNumber, counterFieldNumber, myCounterReadAPIKey);
  int statusCode = 0;
  // Check the status of the read operation to see if it was successful
  statusCode = ThingSpeak.getLastReadStatus();
  if (statusCode == 200) {
    //Serial.println("Thingspeak Read: ");
    Serial.println(count, 5);
  }
  else {
    Serial.println("Problem reading channel. HTTP error code " + String(statusCode));
  }
  //Serial.println("");
  return count;
}

void ThingSpeakPost() {
  unsigned long timerDelay2 = 15000;
  unsigned long lastTime2 = 0;

  if ((millis() - lastTime2) > timerDelay2) {   //Postando os dados no ThingSpeak a cada 15 segundos

    // Set the fields with the values!
    Serial.println("Sending to ThingSpeak:");
    //Serial.println(VARIAVEL);
    //ThingSpeak.setField(CAMPO, VALOR);

    // write to the ThingSpeak channel
    int x = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
    if (x == 200) {
      Serial.println("Channel update successful.");
    }
    else {
      Serial.println("Problem updating channel. HTTP error code " + String(x));
    }

    lastTime2 = millis();
    Serial.println("");
  }
}

//Sub rotina que verifica novos clientes e se sim, envia o HTML:
void http() {
  WiFiClient client = server.available();//Diz ao cliente que há um servidor disponivel.
  if (client) {                          //Se houver clientes conectados, ira enviar o HTML.
    String currentLine = "";

    while (client.connected()) {
      if (client.available()) {
        char c = client.read(); //Leitura dos bytes enviados pelo client
        Serial.write(c);
        header += c; //Os bytes coletados formam a solicitação do client no header

        int i = 0, j = 0;

        if (c == '\n') {
          if (currentLine.length() == 0) {

            //Código HTML que será feito upload no webserver:
            html  = "<!DOCTYPE html><html lang='pt-br'>";
            html += "<head>";
            html += "  <meta charset='UTF-8'>";
            html += "  <meta name='viewport' content='width=device-width, initial-scale=1.0'>";
            html += "  <meta http-equiv='X-UA-Compatible' content='ie=edge'>";
            html += "  <title>Interface Gráfica</title>";

            html += "  <link rel='stylesheet' href='https://maxcdn.bootstrapcdn.com/bootstrap/4.4.1/css/bootstrap.min.css'>";

            html += "  <script src='https://ajax.googleapis.com/ajax/libs/jquery/3.4.1/jquery.min.js'></script>";
            html += "  <script src='https://cdnjs.cloudflare.com/ajax/libs/popper.js/1.16.0/umd/popper.min.js'></script>";
            html += "  <script src='https://maxcdn.bootstrapcdn.com/bootstrap/4.4.1/js/bootstrap.min.js'></script>";
            html += "  <script src='https://ajax.googleapis.com/ajax/libs/jquery/3.4.1/jquery.min.js'></script>";
            html += "  <script src='https://cdnjs.cloudflare.com/ajax/libs/popper.js/1.16.0/umd/popper.min.js'></script>";
            html += "  <script src='https://maxcdn.bootstrapcdn.com/bootstrap/4.4.1/js/bootstrap.min.js'></script>";

            html += "  <style>";
            html += "    .col-sm-8{margin-bottom: 50px;}";
            html += "    .text{color: #8E8E8E;font: normal 13pt Segoe UI;text-align: right;}";
            html += "    .info{color: #605D5D;font: normal 30pt Segoe UI;text-align: left;}";
            html += "    .container{margin-top: 30px;position: relative;}";
            html += "  </style>";
            html += "</head>";
            html += "<body onload='mudarValor()'>";
            html += "  <div class='container mt-3'>";
            html += "    <div class='row'>";
            html += "      <div class='col-sm-3'>";
            html += "        <button type='button' class='btn btn-primary' data-toggle='modal'data-target='#myModal'>";
            html += "          Abrir configurações";
            html += "        </button>";
            html += "      </div>";
            html += "    </div>";
            html += "  </div>";


            html += "  <div class='col-xs- d-flex flex-column justify-content-center'>";
            html += "    <form method='get'>";
            html += "      <p align='middle'>[ Somente números com ponto ]</p>";
            html += "      <h5 align='middle'><label for='meta'>Mudar meta:  </label>";
            html += "      <input type='text' name='meta' id='meta'>";
            html += "      <input class='btn btn-danger mb-1' type='submit' value='Submit'>";
            html += "    </h5></form>";

            if (header.lastIndexOf("?meta=") > 0) {   //Extrair o valor inserido para Meta Atual
              meta = (header.substring(header.lastIndexOf("=") + 1, header.lastIndexOf(".") + 2)).toFloat();
              Serial.println("Meta Atual: ");
              Serial.println(meta);
              Serial.println("META UPDATE");
            }
            else {
              Serial.println("META NOT UPDATED");
            }

            html += "    <h6 align='middle'>Meta atual: R$ ";
            html +=        meta;
            html += "    </h6>";
            html += "    <h6 align='middle'>Consumo no mês: R$ ";
            html +=        dinheiroPorMes;
            html += "    </h6>";
            html += "    <h6 align='middle'>Consumo por dia: R$ ";
            html +=        dinheiroPorDia;
            html += "    </h6>";
            html += "  </div>";


            html += "  <div class='modal fade' id='myModal'>";
            html += "    <div class='modal-dialog'>";
            html += "      <div class='modal-content'>";
            html += "        <div class='modal-header'>";
            html += "          <h4 class='modal-title'>Configurações</h4>";
            html += "          <button type='button' class='close' data-dismiss='modal'> x </button>";
            html += "        </div>";
            html += "        <div class='modal-body'>";
            html += "          <h7>Nome</h7></br>";
            html += "          <h5>IFES Campus Guarapari</h5></br></br>";
            html += "        </div>";
            html += "        <div class='modal-body'>";
            html += "          <h7>Dispositivo</h7></br>";
            html += "          <h5>Microondas</h5>";
            html += "          <h7>Tomada</h7></br>";
            html += "          <h5>127 V</h5></br></br>";
            html += "        </div>";
            html += "        <div class='modal-footer'>";
            html += "          <button type='button' class='btn btn-danger' data-dismiss='modal'>Fechar</button>";
            html += "        </div>";
            html += "      </div>";
            html += "    </div>";
            html += "  </div>";
            html += "  <div class='container'>";
            html += "    <div class='row justify-content-center'>";
            html += "      <div class='col-sm-8 ml-1'>";
            html += "        <canvas id = 'myChart' width='100' height='50'></canvas>";
            html += "      </div>";

            //               botoes que alteram o formato do grafico:
            html += "      <div class='col-xs- d-flex flex-column justify-content-center'>";
            html += "        <a href=\'/day\'><button type='button' class='btn btn-danger mb-1' data-dismiss='modal'>Day</button></a>";
            html += "        <a href=\'/week\'><button type='button' class='btn btn-danger mb-1' data-dismiss='modal'>Week</button></a>";
            html += "        <a href=\'/month\'><button type='button' class='btn btn-danger mb-1' data-dismiss='modal'>Month</button></a>";
            html += "        <a href=\'/year\'><button type='button' class='btn btn-danger mb-1' data-dismiss='modal'>Year</button></a>";
            html += "      </div>";

            //               O "header" contém as mensagens e solicitações do client, abaixo ocorre a
            //              análise se os botões de "day", "month", "week" ou "year" foram aperdados
            //              e realizam as devidas modificações no código html, de acordo com o caso.
            Serial.println(header);
            Serial.println("");
            if (header.indexOf("day") > 0) {
              maxAmount = 24;
              hdmAtual = timeinfo.tm_hour;
              i = timeinfo.tm_hour;
              j = i;
              Serial.println("Botao: DAY");
            }

            else if (header.indexOf("month") > 0) {
              maxAmount = 31;
              hdmAtual = timeinfo.tm_mday;
              i = maxAmount - 1;
              j = i;
              Serial.println("Botao: MONTH");
            }
            else if (header.indexOf("week") > 0) {
              maxAmount = 7;
              hdmAtual = timeinfo.tm_mday;
              i = maxAmount - 1;
              j = i;
              Serial.println("Botao: WEEK");
            }
            else if (header.indexOf("year") > 0) {
              maxAmount = 12;
              hdmAtual = timeinfo.tm_mon;
              i = timeinfo.tm_mon;
              j = i;
              Serial.println("Botao: YEAR");
            }
            header = "\n"; //É necessario limpar o header para que o metodo "indexOf" nao confunda as informacoes
            Serial.println("");


            html += "      </div>";
            html += "    </div>";

            //           Criação do texto 'tensão', 'potencia', 'corrente' e 'fator de potencia' no site:
            html += "    <div class='container'>";
            html += "      <div class='row'>";
            html += "        <div class='col-sm-2'>";
            html += "          <p class='text'>Tensão</p></br>";
            html += "        </div>";
            html += "        <div class='col-sm-4'>";
            html += "          <p class='info' id='tensao'></p>";
            html += "        </div>";
            html += "        <div class='col-sm-2'>";
            html += "          <p class='text'>Potência</p></br>";
            html += "        </div>";
            html += "        <div class='col-sm-4'>";
            html += "          <p class='info' id='potencia'></p>";
            html += "        </div>";
            html += "      </div>";
            html += "    </div>";
            html += "    <div class='container'>";
            html += "      <div class='row'>";
            html += "        <div class='col-sm-2'>";
            html += "          <p class='text'>Corrente</p></br>";
            html += "        </div>";
            html += "        <div class='col-sm-4'>";
            html += "          <p class='info' id='corrente'></p>";
            html += "        </div>";
            html += "        <div class='col-sm-2'>";
            html += "          <p class='text'>Fator de Potência</p></br>";
            html += "        </div>";
            html += "        <div class='col-sm-4'>";
            html += "          <p class='info' id='fp'></p>";
            html += "        </div>";
            html += "      </div>";
            html += "    </div>";

            //       Formatação do grafico:
            html += "<script src='https://cdn.jsdelivr.net/npm/chart.js@2.8.0'></script>";
            html += "<script>";
            html += "          var ctx = document.getElementById('myChart').getContext('2d');";
            html += "          var myChart = new Chart(ctx, {";
            html += "            type: 'line',";
            html += "            data: {";
            html += "              datasets: [{";
            html += "                label: 'Gráfico de Consumo',";
            html += "                backgroundColor: 'rgb(255, 133, 152, 0.2)',";
            html += "                pointBorderColor: 'rgb(255, 133, 152)',";
            html += "                borderColor: 'rgb(255, 133, 152)',";
            html += "                data: [";

            //Para nao exibir "dias negativos" no grafico:
            if (maxAmount > hdmAtual) {
              j = hdmAtual;
              if (maxAmount == 31 || maxAmount == 7) {
                j--;              //Não existe dia "0", então é necessário fazer a correção em "j" decrementando seu valor em 1
              }
              maxAmount = hdmAtual;
            }

            Serial.println("Hora/Dia/Mes: ");
            Serial.print(hdmAtual);
            Serial.println(" ");

            //Preenchimento dos dados referentes ao eixo Y do grafico (consumo):
            Serial.println("");
            Serial.println("   EIXO Y   ");
            for (i; i > -1; i--) {
              // O controle da quantidade de informação a ser exibida no grafico é feito pelas variaveis "maxAmount" e "hdmAtual"
              if (hdmAtual == timeinfo.tm_mday) {             //Formato: Week & Month
                html += consumo_diario[hdmAtual - i];
                Serial.print("week/month");
              }
              else if (hdmAtual == timeinfo.tm_hour) {        //Formato: Day
                html += consumo_hora[hdmAtual - i];
                Serial.print("day");
              }
              else if (hdmAtual == timeinfo.tm_mon) {         //Formato: Year
                html += consumo_mensal[hdmAtual - i];
                Serial.print("year");
              }

              if (i > 0) {
                html += ",";
                Serial.print(", ");
              }
            }
            Serial.println(" ");

            // Preenchimento dos dados referentes ao eixo X do grafico (tempo):
            html += "]}], labels: [";
            Serial.println("   EIXO X   ");
            for (j; j > -1; j--) {              //NOTA: FALTA FAZER CORREÇÃO DO CONFLITO DE QUANDO O DIA É IGUAL AO MES E SITUAÇÕES SIMILARES

              if (hdmAtual == timeinfo.tm_mday) {                 //Formato: Week & Month
                html += "'";
                html += timeinfo.tm_mday - j;                     //Dia do mes
                html += "/";
                html += timeinfo.tm_mon + 1;                      //Mes do ano
                html += "'";
                Serial.print("Week/Month");
              }
              else if (hdmAtual == timeinfo.tm_hour) {            //Formato: Day
                html += "'";
                html += timeinfo.tm_hour - j;                     //Hora do dia
                html += "h'";
                Serial.print("Day");
              }
              else if (hdmAtual == timeinfo.tm_mon) {             //Formato: Year
                html += "'";
                html += meses[timeinfo.tm_mon - j];               //Mes do ano
                html += "'";
                Serial.print("Year");
              }

              if (j > 0) {
                html += ",";     //Separação entre itens dentro do label
                Serial.print(", ");
              }
            }
            Serial.println(" ");
            html += "]},options: {}});</script>";

            //Exibição dos valores de potencia, fator de potencia, tensão e corrente abaixo de grafico:
            html += " <script>function mudarValor(){";
            html += "   setInterval(function(){";
            html += "    document.getElementById('tensao').innerText = ";
            html +=      TENSAO;
            html += "    ;";
            //html += "    Math.random();"; //geração de numero aleatorio
            html += "    document.getElementById('corrente').innerText = ";
            html +=      CORRENTE;
            html += "     ;";
            html += "    document.getElementById('potencia').innerText = ";
            html +=      (TENSAO * CORRENTE);
            html += "    ;";
            html += "    document.getElementById('fp').innerText = ";
            html +=      FP;
            html += "    ;";
            html += "  }, 5000)}";
            html += " </script>";
            html += "</body>";
            html += "</html>";
            Serial.println(" ");
            Serial.println("HTML end");
            client.print(html);//Finalmente, enviamos o HTML para o cliente.
            client.stop();//Encerra a conexao.
          }

          else {
            Serial.println("");
            Serial.println(WiFi.localIP());
          }

        }
      }
    }
  }
}


//função que printa informações de tempo no monitor serial:
void printLocalTime() {
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return;
  }
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
  Serial.print("Day of week: ");
  Serial.println(&timeinfo, "%A");
  Serial.print("Month: ");
  Serial.println(&timeinfo, "%B");
  Serial.print("Day of Month: ");
  Serial.println(&timeinfo, "%d");
  Serial.print("Year: ");
  Serial.println(&timeinfo, "%Y");
  Serial.print("Hour: ");
  Serial.println(&timeinfo, "%H");
  Serial.print("Hour (12 hour format): ");
  Serial.println(&timeinfo, "%I");
  Serial.print("Minute: ");
  Serial.println(&timeinfo, "%M");
  Serial.print("Second: ");
  Serial.println(&timeinfo, "%S");

  Serial.println("Time variables");
  char timeHour[3];
  strftime(timeHour, 3, "%H", &timeinfo);
  Serial.println(timeHour);
  char timeWeekDay[10];
  strftime(timeWeekDay, 10, "%A", &timeinfo);
  Serial.println(timeWeekDay);
  Serial.println();
}

//FUNCOES QUE NAO ESTAO SENDO UTILIZADAS:
/*
  void TariffInfo() {
  if (TariffFlag_number == 0) {
    TariffFlag_name = "Verde";
  }
  else if (TariffFlag_number == 1) {
    TariffFlag_name = "Amarelo";
  }
  else if (TariffFlag_number == 2) {
    TariffFlag_name = "Vermelho P1";
  }
  else if (TariffFlag_number == 3) {
    TariffFlag_name = "Vermelho P2";
  }
  else {
    TariffFlag_name = "Error";
  }

  Serial.println("");
  Serial.println("Bandeira Tarifária:");
  Serial.println(TariffFlag_number);
  Serial.println(TariffFlag_name);
  Serial.println("");

  }
*/

/*

  void APIdoSetorEletrico() {   //ref: https://apidosetoreletrico.com.br/api-docs/index.html
  unsigned long timerDelay = 10000;
  unsigned long lastTime = 0;

  if ((millis() - lastTime) > timerDelay) {   //Coletando informações de tarifa 1 vez a cada 5 segundos
    if (WiFi.status() == WL_CONNECTED) {      // Check WiFi connection status

      String serverPath = "https://apidosetoreletrico.com.br/api/energy-providers/40/tariffs?tariffTypeName=Verde&suppliedVoltageTypeName=B1";
      //String serverPath = "https://apidosetoreletrico.com.br/api/energy-providers/tariff-flags?monthStart=2020-03-01&monthEnd=2020-05-01";

      jsonBuffer = httpGETRequest(serverPath.c_str());  //Coletando o JSON do serverpath
      Serial.println(jsonBuffer);                       //Exibindo o buffer JSON
      JSONVar myObject = JSON.parse(jsonBuffer);

      // JSON.typeof(jsonVar) can be used to get the type of the var
      if (JSON.typeof(myObject) == "undefined") {
        Serial.println("Parsing input failed!");
        return;
      }

      Serial.print("JSON object = ");
      Serial.println(myObject);  //printar o JSON no monitor serial
      TariffFlag_number = myObject["flagType"];
    }
    else {
      Serial.println("WiFi Disconnected");
    }
    lastTime = millis();
  }
  }
*/

/*
  String httpGETRequest(const char* serverName) {
  HTTPClient http;

  // Your IP address with path or Domain name with URL path
  http.begin(serverName);

  // Send HTTP POST request
  int httpResponseCode = http.GET();

  String payload = "{}";

  if (httpResponseCode > 0) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    payload = http.getString();
  }
  else {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
  }
  // Free resources
  http.end();

  return payload;
  }
*/
