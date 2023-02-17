#include "OpenKNX/FlashStorage.h"
#include "OpenKNX/Common.h"

namespace OpenKNX
{
    std::string FlashStorage::logPrefix() {
        return "FlashStorage";
    }

    void FlashStorage::init()
    {
        _flashSize = knx.platform().getNonVolatileMemorySize();
        _flashStart = knx.platform().getNonVolatileMemoryStart();
        _flashEnd = _flashStart + _flashSize;
    }

    void FlashStorage::load()
    {
        init();
        uint32_t start = millis();
        loadedModules = new bool[openknx.getModules()->count];
        logInfoP("Load data from flash");
        logIndentUp();
        readData();
        initUnloadedModules();
        logInfoP("Loading completed (%ims)", millis() - start);
        logIndentDown();
    }

    /**
     * Initialize all modules expecting data in flash, but not loaded yet.
    */
    void FlashStorage::initUnloadedModules()
    {
        Modules *modules = openknx.getModules();
        for (uint8_t i = 0; i < modules->count; i++)
        {
            // check module expectation and load state
            Module *module = modules->list[i];
            const uint8_t moduleId = modules->ids[i];
            const uint16_t moduleSize = module->flashSize();

            if (moduleSize > 0 && !loadedModules[moduleId])
            {
                logDebugP("Init module %s (%i)", module->name().c_str(), moduleId);
                module->readFlash(new uint8_t[0], 0);
            }
        }
    }

    /**
     * Check version/format/checksum and let matching modules read their data from flash-storage
    */
    void FlashStorage::readData()
    {
        // check magicwords exists (at last position)
        _currentReadAddress = _flashEnd - FLASH_DATA_INIT_LEN;
        if (FLASH_DATA_INIT != readInt())
        {
            logInfoP("Abort: No data found");
            return;
        }

        // start reading all other fields in META in order
        _currentReadAddress = _flashEnd - FLASH_DATA_META_LEN;

        // read and check FirmwareVersion/Number
        _lastFirmwareNumber = readWord();
        _lastFirmwareVersion = readWord();
        logDebugP("FirmwareNumber: 0x%04X", _lastFirmwareNumber);
        logDebugP("FirmwareVersion: %i", _lastFirmwareVersion);
        if (_lastFirmwareNumber != openknx.info.firmwareNumber())
        {
            logErrorP("Abort: Data from other application");
            return;
        }

        // read size and infer begin of data storage
        const uint16_t dataSize = readWord();
        uint8_t *dataStart = (_flashEnd - FLASH_DATA_META_LEN - dataSize);

        // validate checksum
        if (!verifyChecksum(dataStart, dataSize + FLASH_DATA_META_LEN - FLASH_DATA_INIT_LEN))
        {
            logErrorP("Abort: Checksum invalid!");
            logHexErrorP(dataStart, dataSize + FLASH_DATA_META_LEN - FLASH_DATA_INIT_LEN);
            return;
        }

        logHexTraceP(dataStart, dataSize + FLASH_DATA_META_LEN);

        uint16_t dataProcessed = 0;
        while (dataProcessed < dataSize)
        {
            _currentReadAddress = dataStart + dataProcessed;
            const uint8_t moduleId = readByte();
            const uint16_t moduleSize = readWord();

            Module *module = openknx.getModule(moduleId);
            if (module == nullptr)
            {
                logDebugP("Skip module with id %i (not found)", moduleId);
            }
            else
            {
                logDebugP("Restore module %s (%i) with %i bytes", module->name().c_str(), moduleId, moduleSize);
                logIndentUp();
                logHexTraceP(_currentReadAddress, moduleSize);

                module->readFlash(_currentReadAddress, moduleSize);
                loadedModules[moduleId] = true;
                logIndentDown();
            }

            dataProcessed += FLASH_DATA_MODULE_ID_LEN + FLASH_DATA_SIZE_LEN + moduleSize;
        }
    }

    void FlashStorage::save(bool force /* = false */)
    {
        init();
        _checksum = 0;

        uint32_t start = millis();

        // table is not loaded (ets prog running) and save is not possible
        if (!knx.configured())
            return;

        // we have to ensure, that save is not called too often, because flash memory
        // does not survive too many writes
        if (!force && _lastWrite > 0 && !delayCheck(_lastWrite, FLASH_DATA_WRITE_LIMIT))
            return;

        logInfoP("Save data to flash%s", force ? " (force)" : "");
        logIndentUp();

        Modules *modules = openknx.getModules();
        
        // calculate the overall required size to store data for all modules
        uint16_t dataSize = 0;
        for (uint8_t i = 0; i < modules->count; i++)
        {
            const uint16_t moduleSize = modules->list[i]->flashSize();
            if (moduleSize > 0)
            {
                dataSize += FLASH_DATA_MODULE_ID_LEN + FLASH_DATA_SIZE_LEN + moduleSize;
            }
        }

        logTraceP("dataSize: %i", dataSize);

        // start point (based on right alignment within available size of flash)
        _currentWriteAddress = _flashSize - dataSize - FLASH_DATA_META_LEN;

        logTraceP("startPosition: %i", _currentWriteAddress);

        // write data for all modules
        for (uint8_t i = 0; i < modules->count; i++)
        {
            Module *module = modules->list[i];
            const uint16_t moduleSize = module->flashSize();
            if (moduleSize > 0)
            {
                const uint8_t moduleId = modules->ids[i];
                logDebugP("Save module %s (%i) with %i bytes", module->name().c_str(), moduleId, moduleSize);

                // write header for module data
                _maxWriteAddress = _currentWriteAddress + FLASH_DATA_MODULE_ID_LEN + FLASH_DATA_SIZE_LEN;
                writeByte(moduleId);
                writeWord(moduleSize);

                // write the module data
                _maxWriteAddress += moduleSize;
                module->writeFlash();
                writeFilldata();
            }
        }

        // block of metadata
        _maxWriteAddress = _currentWriteAddress + FLASH_DATA_META_LEN;

        // application info
        writeWord(openknx.info.firmwareNumber());
        writeWord(openknx.info.firmwareVersion());

        // write size
        writeWord(dataSize);

        // write checksum
        writeWord(_checksum);

        // write init
        writeInt(FLASH_DATA_INIT);

        knx.platform().commitNonVolatileMemory();
        logHexTraceP(_flashEnd - dataSize - FLASH_DATA_META_LEN, dataSize + FLASH_DATA_META_LEN);

        _lastWrite = millis();
        logInfoP("Save completed (%ims)", _lastWrite - start);
        logIndentDown();

    }

    uint16_t FlashStorage::calcChecksum(uint8_t *data, uint16_t size)
    {
        uint16_t sum = 0;

        for (uint16_t i = 0; i < size; i++)
            sum = sum + data[i];

        return sum;
    }

    bool FlashStorage::verifyChecksum(uint8_t *data, uint16_t size)
    {
        // logInfoP("verifyChecksum %i == %i", ((data[size - 2] << 8) + data[size - 1]), calcChecksum(data, size - 2));
        return ((data[size - 2] << 8) + data[size - 1]) == calcChecksum(data, size - 2);
    }

    void FlashStorage::write(uint8_t *buffer, uint16_t size)
    {
        if ((_currentWriteAddress + size) > _maxWriteAddress)
        {
            logErrorP("    A module has tried to write more than it was allowed to write");
            return;
        }

        for (uint16_t i = 0; i < size; i++)
            _checksum += buffer[i];

        _currentWriteAddress = knx.platform().writeNonVolatileMemory(_currentWriteAddress, buffer, size);
    }

    void FlashStorage::write(uint8_t value, uint16_t size)
    {
        if ((_currentWriteAddress + size) > _maxWriteAddress)
        {
            logErrorP("    A module has tried to write more than it was allowed to write");
            return;
        }

        for (uint16_t i = 0; i < size; i++)
            _checksum += value;

        _currentWriteAddress = knx.platform().writeNonVolatileMemory(_currentWriteAddress, value, size);
    }

    void FlashStorage::writeByte(uint8_t value)
    {
        uint8_t buffer[1];
        buffer[0] = value;
        write(buffer);
    }

    void FlashStorage::writeWord(uint16_t value)
    {
        uint8_t buffer[2];
        buffer[0] = ((value >> 8) & 0xff);
        buffer[1] = (value & 0xff);
        write(buffer, 2);
    }

    void FlashStorage::writeInt(uint32_t value)
    {
        uint8_t buffer[4];
        buffer[0] = ((value >> 24) & 0xff);
        buffer[1] = ((value >> 16) & 0xff);
        buffer[2] = ((value >> 8) & 0xff);
        buffer[3] = (value & 0xff);
        write(buffer, 4);
    }

    void FlashStorage::writeFilldata()
    {
        uint16_t fillSize = (_maxWriteAddress - _currentWriteAddress);
        if (fillSize == 0)
            return;

        logTraceP("    writeFilldata %i", fillSize);
        write((uint8_t)FLASH_DATA_FILLBYTE, fillSize);
    }

    uint8_t *FlashStorage::read(uint16_t size /* = 1 */)
    {
        uint8_t *address = _currentReadAddress;
        _currentReadAddress += size;
        return address;
    }

    uint8_t FlashStorage::readByte()
    {
        return read(1)[0];
    }

    uint16_t FlashStorage::readWord()
    {
        return (readByte() << 8) + readByte();
    }

    uint32_t FlashStorage::readInt()
    {
        return (readByte() << 24) + (readByte() << 16) + (readByte() << 8) + readByte();
    }

    uint16_t FlashStorage::firmwareVersion()
    {
        return _lastFirmwareVersion;
    }
} // namespace OpenKNX