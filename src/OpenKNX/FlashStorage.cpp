#include "OpenKNX/FlashStorage.h"
#include "OpenKNX/Common.h"

namespace OpenKNX
{
    FlashStorage::FlashStorage()
    {}

    void FlashStorage::load()
    {
        _flashSize = knx.platform().getNonVolatileMemorySize();
        _flashStart = knx.platform().getNonVolatileMemoryStart();
        uint32_t start = millis();
        loadedModules = new bool[openknx.getModules()->count];
        openknx.log("FlashStorage", "load");
        readData();
        initUnloadedModules();
        openknx.log("FlashStorage", "  complete (%i)", millis() - start);
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
                openknx.log("FlashStorage", "  init module %s (%i)", module->name().c_str(), moduleId);
                module->readFlash(new uint8_t[0], 0);
            }
        }
    }

    /**
     * Check version/format/checksum and let matching modules read their data from flash-storage
    */
    void FlashStorage::readData()
    {
        uint8_t *_flashEnd = _flashStart + _flashSize;
        
        // check magicwords exists (at last position)
        _currentReadAddress = _flashEnd - FLASH_DATA_INIT_LEN;
        if (FLASH_DATA_INIT != readInt())
        {
            openknx.log("FlashStorage", "   - Abort: No data found");
            return;
        }

        // start reading all other fields in META in order
        _currentReadAddress = _flashEnd - FLASH_DATA_META_LEN;

        // read and check FirmwareVersion/Number
        _lastFirmwareNumber = readWord();
        _lastFirmwareVersion = readWord();
        openknx.log("FlashStorage", "  FirmwareNumber: 0x%04X", _lastFirmwareNumber);
        openknx.log("FlashStorage", "  FirmwareVersion: %i", _lastFirmwareVersion);
        if (_lastFirmwareNumber != openknx.info.firmwareNumber())
        {
            openknx.log("FlashStorage", "  - Abort: Data from other application");
            return;
        }

        // read size and infer begin of data storage
        const uint16_t dataSize = readWord();
        uint8_t *dataStart = (_flashEnd - FLASH_DATA_META_LEN - dataSize);

        // validate checksum
        if (!verifyChecksum(dataStart, dataSize + FLASH_DATA_META_LEN - FLASH_DATA_INIT_LEN))
        {
            openknx.log("FlashStorage", "   - Abort: Checksum invalid!");
            openknx.logHex("FlashStorage", dataStart, dataSize + FLASH_DATA_META_LEN - FLASH_DATA_INIT_LEN);
            return;
        }

#ifdef FLASH_DATA_TRACE
        openknx.logHex("FlashStorage", dataStart, dataSize + FLASH_DATA_META_LEN);
#endif

        uint16_t dataProcessed = 0;
        while (dataProcessed < dataSize)
        {
            _currentReadAddress = dataStart + dataProcessed;
            const uint8_t moduleId = readByte();
            const uint16_t moduleSize = readWord();

            Module *module = openknx.getModule(moduleId);
            if (module == nullptr)
            {
                openknx.log("FlashStorage", "  skip module with id %i (not found)", moduleId);
            }
            else
            {
                openknx.log("FlashStorage", "  restore module %s (%i) with %i bytes", module->name().c_str(), moduleId, moduleSize);
#ifdef FLASH_DATA_TRACE
                openknx.logHex("FlashStorage", _currentReadAddress, moduleSize);
#endif
                module->readFlash(_currentReadAddress, moduleSize);
                loadedModules[moduleId] = true;
            }
            
            dataProcessed += FLASH_DATA_MODULE_ID_LEN + FLASH_DATA_SIZE_LEN + moduleSize;
        }
    }

    void FlashStorage::save(bool force /* = false */)
    {
        _checksum = 0;
        _flashSize = knx.platform().getNonVolatileMemorySize();
        _flashStart = knx.platform().getNonVolatileMemoryStart();

        uint32_t start = millis();

        // table is not loaded (ets prog running) and save is not possible
        if (!knx.configured())
            return;

        // we have to ensure, that save is not called too often, because flash memory
        // does not survive too many writes
        if (!force && _lastWrite > 0 && !delayCheck(_lastWrite, FLASH_DATA_WRITE_LIMIT))
            return;

        openknx.log("FlashStorage", "save <%i>", force);

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

#ifdef FLASH_DATA_TRACE
        openknx.log("FlashStorage", "  dataSize: %i", dataSize);
#endif

        // start point (based on right alignment within available size of flash)
        _currentWriteAddress = _flashSize - dataSize - FLASH_DATA_META_LEN;

#ifdef FLASH_DATA_TRACE
        openknx.log("FlashStorage", "  startPosition: %i", _currentWriteAddress);
#endif

        for (uint8_t i = 0; i < modules->count; i++)
        {
            Module *module = modules->list[i];
            const uint16_t moduleSize = module->flashSize();
            const uint8_t moduleId = modules->ids[i];

            if (moduleSize == 0)
                continue;

            // write data
            _maxWriteAddress = _currentWriteAddress +
                               FLASH_DATA_MODULE_ID_LEN +
                               FLASH_DATA_SIZE_LEN;

            writeByte(moduleId);

            writeWord(moduleSize);

            _maxWriteAddress = _currentWriteAddress + moduleSize;

            openknx.log("FlashStorage", "  save module %s (%i) with %i bytes", module->name().c_str(), moduleId, moduleSize);
            module->writeFlash();
            writeFilldata();
        }

        // write magicword
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
#ifdef FLASH_DATA_TRACE
        openknx.logHex("FlashStorage", _flashStart + _flashSize - dataSize - FLASH_DATA_META_LEN, dataSize + FLASH_DATA_META_LEN);
#endif

        _lastWrite = millis();
        openknx.log("FlashStorage", "  complete (%ims)", _lastWrite - start);
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
        // openknx.log("FlashStorage", "verifyChecksum %i == %i", ((data[size - 2] << 8) + data[size - 1]), calcChecksum(data, size - 2));
        return ((data[size - 2] << 8) + data[size - 1]) == calcChecksum(data, size - 2);
    }

    void FlashStorage::write(uint8_t *buffer, uint16_t size)
    {
        if ((_currentWriteAddress + size) > _maxWriteAddress)
        {
            openknx.log("FlashStorage", "write not allowed");
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
            openknx.log("FlashStorage", "write not allowed");
            return;
        }

        for (uint16_t i = 0; i < size; i++)
            _checksum += value;

        _currentWriteAddress = knx.platform().writeNonVolatileMemory(_currentWriteAddress, value, size);
    }

    void FlashStorage::writeByte(uint8_t value)
    {
        uint8_t *buffer = new uint8_t[1];
        buffer[0] = value;
        write(buffer);
        delete buffer;
    }

    void FlashStorage::writeWord(uint16_t value)
    {
        uint8_t *buffer = new uint8_t[2];
        buffer[0] = ((value >> 8) & 0xff);
        buffer[1] = (value & 0xff);
        write(buffer, 2);
        delete buffer;
    }

    void FlashStorage::writeInt(uint32_t value)
    {
        uint8_t *buffer = new uint8_t[4];
        buffer[0] = ((value >> 24) & 0xff);
        buffer[1] = ((value >> 16) & 0xff);
        buffer[2] = ((value >> 8) & 0xff);
        buffer[3] = (value & 0xff);
        write(buffer, 4);
        delete buffer;
    }

    void FlashStorage::writeFilldata()
    {
        uint16_t fillSize = (_maxWriteAddress - _currentWriteAddress);
        if (fillSize == 0)
            return;

#ifdef FLASH_DATA_TRACE
        openknx.log("FlashStorage", "    writeFilldata %i", fillSize);
#endif
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