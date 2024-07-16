# Railuino
 Rewrite of version 0.9.0 of Joerg Pleumann's Railuino library

 https://youtu.be/x5fVPsChlIA

Version 0.9.1 de la bibliothèque Railuino de Joerg Pleumann dont le dépôt git se trouve à cet emplacement : https://code.google.com/archive/p/railuino/

Cette bibliothèque est sous licence GNU LESSER GENERAL PUBLIC LICENSE - Version 2.1, February 1999

Copyright (C) 2012 Joerg Pleumann

Cette version 9.0.1 est une réécriture profonde à partir de la version 0.9.0, dernière version publiée à ma connaissance.

Dans sa version 0.9.0, cette bibliothèque est très bien écrite mais elle est ancienne (2013, c’est beaucoup en programmation), elle n’est pas adaptée aux nouveaux microcontrôleurs comme l’ESP32, comporte quelques manques (comme la reconnaissance automatique en MFX) et doit être optimisée par rapport aux normes C++ 11 et suivantes. Je suis actuellement en train de réécrire certaines parties de cette bibliothèque que je teste au fur et à mesure.

Je ne m’intéresse actuellement qu’à la version ESP32. Je publie mon travail sur ce git à chaque évolution majeure.

Les exemples originaux pour la mise en œuvre et les tests de cette bibliothèque sont encore d’actualité mais doivent aussi être adaptés.

Vous trouverez dans le dossier « exemples » les fichiers mis à jour qui portent les noms originaux auxquels j’ai ajouté « _new ».

Les fichiers originaux sont conservés à titre de mémoire et de comparaison.

*****************************************************************************************************

Version 0.9.1 of Joerg Pleumann's Railuino library, the git repository for which can be found at: https://code.google.com/archive/p/railuino/

This library is licensed under the GNU LESSER GENERAL PUBLIC LICENSE - Version 2.1, February 1999

Copyright (C) 2012 Joerg Pleumann

This version 9.0.1 is a thorough rewrite of version 0.9.0, the last version published to my knowledge.

In its version 0.9.0, this library is very well written but it is old (2013 is a long time in programming), it is not adapted to new microcontrollers such as the ESP32, has a few shortcomings (such as automatic recognition in MFX) and needs to be optimised in relation to the C++ 11 and later standards. I'm currently rewriting parts of this library and testing them as I go along.

I'm currently only interested in the ESP32 version. I publish my work on this git whenever there is a major change.

The original examples for implementing and testing this library are still relevant, but they also need to be adapted.

You'll find the updated files in the "examples" folder, which have the original names to which I've added "_new".

The original files are kept for memory and comparison.




TrackController Class Documentation
Overview

The TrackController class abstracts the communication with Märklin model train components using a CAN bus interface. It provides high-level methods for controlling locomotives, turnouts, and accessories, while managing low-level message handling internally. This documentation summarizes the class's functionality and usage patterns for interfacing with model train systems.

Dependencies
Arduino.h: Arduino standard library.
TrackMessage.h: Header for defining CAN message structure.
Config.h: Configuration file (not provided) for specific settings.

Constructors
TrackController();
Default constructor initializes the controller with default settings.
TrackController(uint16_t hash, bool debug);
Constructor with parameters to initialize the controller with a specific hash and debug mode.
Destructor
~TrackController();
Cleans up resources when the object is destroyed.

Public Methods
Initialization and Configuration
void init(uint16_t hash, bool debug, bool loopback);

Initializes the controller with a specified hash, debug mode, and loopback mode.
void begin();

Initializes the CAN hardware and starts message reception.
void end();

Stops message reception and clears the internal message buffer.
void generateHash();

Generates a unique hash for the controller to avoid conflicts on the CAN bus.
Power and Control
bool setPower(bool power);

Controls the power state of the track. Stops all locomotives when power is false.
bool setLocoDirection(uint16_t address, uint8_t direction);

Sets the direction of a locomotive identified by address.
bool toggleLocoDirection(uint16_t address);

Toggles the direction of a locomotive identified by address.
bool setLocoSpeed(uint16_t address, uint16_t speed);

Sets the speed of a locomotive identified by address.
bool setLocoFunction(uint16_t address, uint8_t function, uint8_t power);

Sets a specific function (e.g., lights) of a locomotive.
bool toggleLocoFunction(uint16_t address, uint8_t function);

Toggles a function (on/off) of a locomotive.
bool setAccessory(uint16_t address, uint8_t position, uint8_t power, uint16_t time);

Controls a magnetic accessory (e.g., turnout).
bool setTurnout(uint16_t address, bool straight);

Controls a turnout to be straight or curved.
Query Methods
bool getLocoDirection(uint16_t address, uint8_t *direction);

Queries the direction of a locomotive identified by address.
bool getLocoSpeed(uint16_t address, uint16_t *speed);

Queries the speed of a locomotive identified by address.
bool getLocoFunction(uint16_t address, uint8_t function, uint8_t *power);

Queries the status of a specific function of a locomotive.
bool getAccessory(uint16_t address, uint8_t *position, uint8_t *power);

Queries the state of a magnetic accessory.
Configuration Methods
bool writeConfig(uint16_t address, uint16_t number, uint8_t value);

Writes a configuration value to a locomotive.
bool readConfig(uint16_t address, uint16_t number, uint8_t *value);

Reads a configuration value from a locomotive.
Utility Methods
bool exchangeMessage(TrackMessage &out, TrackMessage &in, uint16_t timeout);

Sends a message and waits for a response within the specified timeout.
bool getVersion(uint8_t *high, uint8_t *low);

Queries the software version of the track format processor.

Member Variables
uint16_t mHash: Hash of the controller instance.
bool mDebug: Debug mode flag.
bool mLoopback: Loopback mode flag.




