#include <RelaisManager.h>
#include <Ethernet.h>
#include <SPI.h>
#include <WebServer.h>
#include <Wire.h>
#include "Timer.h"
#include "RTClib.h"

RelaisManager cafetiereInterupteur(8, 9);
const int temperatureCapteur = A0;
const int rgb[3] = {3, 5, 6};

RTC_DS1307 RTC;

bool started = false;

IPAddress ipLocal(192, 168, 0, 201);
byte mac[] = { 0x90, 0xA2, 0xDA, 0x0F, 0x50, 0x4E };

int cafeStartHour = 0;
int cafeStartMinute = 0;
bool cafeStartActivated = false;

int cafeSecondeBeforeStop = 0;

WebServer webserver("", 80);

Timer t;


/**
 * FONCTIONS
 *
 */

/**********************/
/** HTTP POST SYSTEM **/
/**********************/

// Variable pour stocker les noms des donnees
char postNameDatas[16][16];
// Variable pour stocker les valeurs des donnees
char postValueDatas[16][16];
// Variable de la taille des donnees
int postDatasSize = 0;

/**
 *
 * Recupere les donnees de la requete courrante POST
 * et les stock dans un tableau et retourne celle demandee
 * @param WebServer server Instance la classe Webserver de la requete courante
 * @param String wanted Le nom de la donnee que l'on veut recuperer
 *
 * @return String
 *
 */
String getPostDatas(WebServer &server, String wanted)
{
  bool repeat = true;
  char name[16], value[16];

  // Si aucune donnee n'est stockee dans le tableau on parcour celle de la requete
  if (postDatasSize <= 0) {
    // Tant que tout n'est pas lu
    while(repeat) {
      repeat = server.readPOSTparam(name, 16, value, 16);
      String strName(name);

      if (strName != "") {
        // On stock le nom de la donnee
        strcpy(postNameDatas[postDatasSize], name);
        // On stock la valeur de la donnee
        strcpy(postValueDatas[postDatasSize], value);
        
        postDatasSize++;
      }
    }
  }

  // On parcoure le tableau de stockage des donnees
  for(int i = 0; i < postDatasSize; i++) {
    char * dataName = postNameDatas[i];
    char * dataValue = postValueDatas[i];

    String strDataName(dataName);
    String strDataValue(dataValue);

    // Si la donnee courante correspond a la donnee demande
    // on retourne sa valeur a l'utilisateur
    if (strDataName == wanted) {
      return dataValue;
    }
  }
  
  return "";
  
}

/**
 *
 * Nettoie le tableau de donnees POST
 * A executer en chaque fin de requete HTTP
 *
 */
void cleanPostDatas()
{
  char empty[1];

  for (int i = 0; i < postDatasSize; i++) {
    strcpy(postNameDatas[i], empty);
    strcpy(postValueDatas[i], empty);
  }

  postDatasSize = 0;
}


/**
 * Recupere la temperature
 * @return int
 */
int getTemperature()
{
    float rawTemperature = analogRead(temperatureCapteur);
    float voltage = (rawTemperature * 5.0) / 1024.0;
    return voltage * 100;
}

/**
 * Recupere le temps de cafe restant
 * @return int
 *
 */
int getTimeLeft()
{
    DateTime now = RTC.now();
    int left = cafeSecondeBeforeStop - now.unixtime();

    return left / 60;
}

/**
 * Allume les leds avec le code couleur demande
 *
 */
void lightRgb(int r, int g, int b)
{
    analogWrite(rgb[0], r);
    analogWrite(rgb[1], g);
    analogWrite(rgb[2], b);
}

/**
 * Verifie et modifie l'etat des leds
 *
 */
void verifyLedState()
{
    int temperature = getTemperature();
    
    if (temperature >= 50) {
        lightRgb(255, 0, 0);
    } else {
        lightRgb(0, 0, 255);
    }
}

/**
 * Verifie l'heure et les minutes
 *
 */
void verifyHourAndMinute()
{
    DateTime now = RTC.now();
    int current_hour = now.getHour();
    int current_minute = now.getMinute();

    if (cafeStartActivated) {
        if (cafeStartMinute == current_hour && cafeStartMinute == curent_minute) {
            cafeStartActivated = false;
            startCafetiere();
        }
    }
}

/**
 * Stop la cafetiere
 *
 */
void stopCafetiere()
{
    if (started == true) {
        cafetiereInterupteur.off();
        started = false;
        cafeSecondeBeforeStop = 0;
    }
}

/**
 * Demarre la cafetiere
 *
 */
void startCafetiere()
{
    started = true;
    
    // On definie a temps actuel + 15min le decompte
    DateTime now = RTC.now();
    int timestamp = now.unixtime();
    cafeSecondeBeforeStop = timestamp + 960;

    cafetiereInterupteur.on();
}


/**
 *
 * URL DE L'API
 *
 */

/**
 *
 * Demarre la cafetiere
 * /start
 *
 */
void startCmd(WebServer &server, WebServer::ConnectionType type, char *url_tail, bool tail_complete)
{
    startCafetiere();

    // On dit a la cafetiere de s'eteindre dans 15min
    t.after(900000, stopCafetiere);

    server.httpSuccess("application/json");
    server.print("{\"message\": \"Correctement allumée\"}");
}

/**
 *
 * Recupere les infos
 * /infos
 *
 */
void infosCmd(WebServer &server, WebServer::ConnectionType type, char *url_tail, bool tail_complete)
{
    server.httpSuccess("application/json");
    server.print("{\"temperature\":");
    server.print(getTemperature());
    server.print(", \"on\":");
    server.print(started);

    if (started) {
        server.print(", ");
        server.print("\"timeLeft\":");
        server.print(getTimeLeft());
    }

    server.print("}");
}

/**
 *
 * Definie l'heure d'allumage
 * /hours
 *
 */
void hoursCmd(WebServer &server, WebServer::ConnectionType type, char *url_tail, bool tail_complete)
{
    String rawCafeStartHour = getPostDatas(server, "hour");
    cafeStartHour = rawCafeStartHour.toInt();
    
    String rawCafeStartMinute = getPostDatas(server, "minute");
    cafeStartMinute = rawCafeStartMinute.toInt();

    String rawCafeStartActivated = getPostDatas(server, "activated");
    cafeStartActivated = rawCafeStartActivated.toInt();

    server.httpSuccess("application/json");
    server.print("{\"message\": \"Heure correctement modifiée\"}");
}


void setup()
{
    Serial.begin(115200);

    Ethernet.begin(mac, ipLocal);
    webserver.addCommand("infos", &infosCmd);
    webserver.addCommand("start", &startCmd);
    webserver.addCommand("hours", &startCmd);

    // Configuration de la sonde de temperature
    pinMode(temperatureCapteur, INPUT);

    // Configuration des LEDs
    for (int i = 0; i < sizeof(rgb); i++) {
        pinMode(rgb[i], OUTPUT);
    }

    // Debut de l'horloge
    RTC.begin();
    Wire.begin();

    // Si l'horloge n'est pas configuree
    if (!RTC.isrunning()) {
        RTC.adjust(DateTime(__DATE__, __TIME__));
    }

    cafetiereInterupteur.off();

    // On dit de verifier les LEDs tous les secondes
    t.every(1000, verifyLedState);

    // On dit de verifier l'heure et les minutes toutes les 1min
    // pour savoir si la cafetiere a ete programmee
    t.every(60000, verifyHourAndMinute);
}


void loop()
{
    webserver.processConnection();
    
    t.update();
}
