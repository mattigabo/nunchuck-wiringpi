//
// Created by Matteo Gabellini on 2018-12-05.
//

#ifndef NUNCHUCKADAPTER_NUNCHUCKREADER_H
#define NUNCHUCKADAPTER_NUNCHUCKREADER_H

#include <thread>
#include "nunchuckdata.h"
#include "platformchecker.h"

#ifdef __RASPBERRYPI_PLATFORM__
#include <wiringPi.h>
#include <wiringPiI2C.h>
#else
#include "mockedwiringpi.h"
#endif

namespace nunchuckadapter{

    typedef struct RawNunchuckData{
        int joystickPositionX;
        int joystickPositionY;
        int accelerationOnX;
        int accelerationOnY;
        int accelerationOnZ;
        int buttonCState;
        int buttonZState;
    } RawNunchuckData;

    /**
    * This class encapsulate the interaction with Nunchuck ciurcuit through I2C protocol.
    * */
    class NunchuckReader {
    public:
        enum InitializationMode {
            ENCRYPTED,
            NOT_ENCRYPTED
        };
        static const int NUNCHUCK_I2C_ADDRESS = 0x52;
        static const int MINIMUN_CIRCUIT_ADAPTATION_WAIT_MICROSECONDS = 300;
        static const int DEFAULT_CIRCUIT_ADAPTATION_WAIT_MICROSECONDS = 500;

        /**
         *  Create a Nunchuck reader object.
         *  Specify the circuit adaptation microseconds that you want to wait at every interaction with the Nunchuck device.
         *  In order to avoid problems this values can't be less than 300 microseconds.
         * */
        NunchuckReader(const int circuitAdaptationWaitMicroseconds, const InitializationMode initializationMode){
            if(circuitAdaptationWaitMicroseconds < MINIMUN_CIRCUIT_ADAPTATION_WAIT_MICROSECONDS){
                throw "The minimun circuit adapdation wait time is "
                + std::to_string(NunchuckReader::MINIMUN_CIRCUIT_ADAPTATION_WAIT_MICROSECONDS)
                + " microseconds. Please specify a value greater than or equal to"
                + std::to_string(NunchuckReader::MINIMUN_CIRCUIT_ADAPTATION_WAIT_MICROSECONDS);
            }
            this->circuitAdaptationWaitMicrosecons = circuitAdaptationWaitMicroseconds;

            initI2C();

            switch (initializationMode){
                case InitializationMode::ENCRYPTED:
                    initWithEncryption();
                    break;
                case InitializationMode::NOT_ENCRYPTED:
                    initWithoutEncryption();
                    break;
                default:
                    throw "Invalid initialization mode";
            }
        }

        /**
         * Create a Nunchuck reader object where the circuit adaptation wait microseconds is automatic set to
         * the default value specified in the relative costant
         * */
        explicit NunchuckReader(const InitializationMode initializationMode):
        NunchuckReader(NunchuckReader::DEFAULT_CIRCUIT_ADAPTATION_WAIT_MICROSECONDS, initializationMode){}


        /**
         * Check if the Nunchuck has been initialized with enryction mode
         * */
        bool isEncryptedModeEnabled() {
            return encyptedModeEnabled;
        }

        /**
         * Read the values of the nunchuck.
         * The read require minimun the microseconds specified in the constructor as
         * "circuitAdaptationWaitMicroseconds".
         * @return an NunchuckData object that contains the data
         * read from the device.
         * */
        NunchuckData readDeviceValues(){
            RawNunchuckData rawData = readRawData();

            NunchuckJoystick joystick = NunchuckJoystick(rawData.joystickPositionX, rawData.joystickPositionY);
            NunchuckAccelerometer accelerometer = NunchuckAccelerometer(rawData.accelerationOnX, rawData.accelerationOnY, rawData.accelerationOnZ);
            NunchuckButton buttonC = NunchuckButton(rawData.buttonCState);
            NunchuckButton buttonZ = NunchuckButton(rawData.buttonZState);

            return NunchuckData(joystick, accelerometer, buttonZ, buttonC);
        };


        /**
         * Read the raw integer data values from the nunchuck.
         * The read require minimun the microseconds specified in the constructor as
         * "circuitAdaptationWaitMicroseconds".
         * @return a RawNunchuckData with the parsed data from the device buffer into the corresponding integer values
         * */
        RawNunchuckData readRawData(){
            int readBuffer[6];
            fetchDeviceBuffer(readBuffer);
            return parseDeviceBuffer(readBuffer);
        }

    private:
        bool encyptedModeEnabled;
        int i2CPortFileDescriptor;
        int circuitAdaptationWaitMicrosecons;

        void initI2C(){
            i2CPortFileDescriptor = wiringPiI2CSetup(NunchuckReader::NUNCHUCK_I2C_ADDRESS);
            if (i2CPortFileDescriptor < 0) {
                throw "Error during the setup of the I2C communication with the Nunchuck";
            }
        }

        void initWithEncryption(){
            wiringPiI2CWriteReg8(i2CPortFileDescriptor, 0x40, 0x00);
            std::this_thread::sleep_for(std::chrono::microseconds(circuitAdaptationWaitMicrosecons));
            encyptedModeEnabled = true;
        }

        void initWithoutEncryption(){
            wiringPiI2CWriteReg8(i2CPortFileDescriptor, 0xF0, 0x55);
            wiringPiI2CWriteReg8(i2CPortFileDescriptor, 0xFB, 0x00);
            std::this_thread::sleep_for(std::chrono::microseconds(circuitAdaptationWaitMicrosecons));
            encyptedModeEnabled = false;
        }

        /*
         * Decryption function for bytes read from the Nunchuck when
         * it is inizialized with the encryption mode
         * */
        int decrypt(const int readByte){
            return (readByte ^ 0x17) + 0x17;
        }

        void fetchDeviceBuffer(int *readBuffer) {
            wiringPiI2CWrite(i2CPortFileDescriptor, 0x00);
            std::this_thread::sleep_for(std::chrono::microseconds(circuitAdaptationWaitMicrosecons));

            int i;
            for (i=0; i<6; i++) {
                readBuffer[i] = wiringPiI2CRead(i2CPortFileDescriptor);
            }
        };

        RawNunchuckData parseDeviceBuffer(int *readBuffer){
            int joyX = readBuffer[0];
            int joyY = readBuffer[1];

            int rawAccelX = (readBuffer[2] << 2) | ((readBuffer[5] & 0xc0) >> 6);
            int rawAccelY = (readBuffer[3] << 2) | ((readBuffer[5] & 0x30) >> 4);
            int rawAccelZ = (readBuffer[4] << 2) | ((readBuffer[5] & 0x0c) >> 2);

            int c = (readBuffer[5] & 0x02) >> 1;

            int z = readBuffer[5] & 0x01;

            return {joyX, joyY, rawAccelX, rawAccelY, rawAccelZ, c , z};
        }
    };
}

#endif //NUNCHUCKADAPTER_NUNCHUCKREADER_H
