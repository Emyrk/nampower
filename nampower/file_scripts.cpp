#include "file_scripts.hpp"

#include "helper.hpp"
#include "offsets.hpp"

#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <wincrypt.h>
#include <windows.h>

#pragma comment(lib, "Crypt32.lib")

namespace Nampower {
    static constexpr const char *kEncryptedPasswordPrefix = ":encrypted:";

    static bool IsValidFilename(const char *name) {
        return name && name[0] != '\0' && std::strpbrk(name, "<>:\"/\\|?*") == nullptr;
    }

    static bool ValidateLuaFilename(uintptr_t *luaState, const char *filename) {
        if (!IsValidFilename(filename)) {
            lua_error(luaState, "Invalid or empty filename (must not contain: < > : \" / \\ | ?*)");
            return false;
        }
        return true;
    }

    static bool GetEnvVar(const char *name, std::string &outValue) {
        const DWORD valueLen = GetEnvironmentVariableA(name, nullptr, 0);
        if (valueLen == 0) {
            return false;
        }

        std::vector<char> buffer(valueLen);
        const DWORD copied = GetEnvironmentVariableA(name, buffer.data(), valueLen);
        if (copied == 0 || copied >= valueLen) {
            return false;
        }

        outValue.assign(buffer.data(), copied);
        return true;
    }

    static bool Base64Decode(const std::string &input, std::vector<BYTE> &outBytes) {
        DWORD decodedLen = 0;
        if (!CryptStringToBinaryA(input.c_str(), static_cast<DWORD>(input.size()), CRYPT_STRING_BASE64, nullptr,
                                  &decodedLen, nullptr, nullptr)) {
            return false;
        }

        outBytes.resize(decodedLen);
        if (!CryptStringToBinaryA(input.c_str(), static_cast<DWORD>(input.size()), CRYPT_STRING_BASE64, outBytes.data(),
                                  &decodedLen, nullptr, nullptr)) {
            return false;
        }
        outBytes.resize(decodedLen);
        return true;
    }

    static bool Base64Encode(const BYTE *data, DWORD dataLen, std::string &outBase64) {
        DWORD encodedLen = 0;
        if (!CryptBinaryToStringA(data, dataLen, CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, nullptr, &encodedLen)) {
            return false;
        }

        std::vector<char> encoded(encodedLen);
        if (!CryptBinaryToStringA(data, dataLen, CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, encoded.data(),
                                  &encodedLen)) {
            return false;
        }

        if (encodedLen > 0 && encoded[encodedLen - 1] == '\0') {
            --encodedLen;
        }
        outBase64.assign(encoded.data(), encodedLen);
        return true;
    }

    static bool EncryptDpapiBase64(const std::string &plaintext, const std::string &entropy,
                                   std::string &outCiphertextBase64) {
        DATA_BLOB plainBlob = {};
        plainBlob.pbData = reinterpret_cast<BYTE *>(const_cast<char *>(plaintext.data()));
        plainBlob.cbData = static_cast<DWORD>(plaintext.size());

        DATA_BLOB entropyBlob = {};
        if (!entropy.empty()) {
            entropyBlob.pbData = reinterpret_cast<BYTE *>(const_cast<char *>(entropy.data()));
            entropyBlob.cbData = static_cast<DWORD>(entropy.size());
        }

        DATA_BLOB encryptedBlob = {};
        if (!CryptProtectData(&plainBlob, nullptr, entropy.empty() ? nullptr : &entropyBlob, nullptr, nullptr, 0,
                              &encryptedBlob)) {
            return false;
        }

        const bool encoded = Base64Encode(encryptedBlob.pbData, encryptedBlob.cbData, outCiphertextBase64);
        SecureZeroMemory(encryptedBlob.pbData, encryptedBlob.cbData);
        LocalFree(encryptedBlob.pbData);
        return encoded;
    }

    static bool DecryptDpapiBase64(const std::string &ciphertextBase64, const std::string &entropy,
                                   std::string &outPlaintext) {
        std::vector<BYTE> encryptedBytes;
        if (!Base64Decode(ciphertextBase64, encryptedBytes)) {
            return false;
        }

        DATA_BLOB encryptedBlob = {};
        encryptedBlob.pbData = encryptedBytes.data();
        encryptedBlob.cbData = static_cast<DWORD>(encryptedBytes.size());

        DATA_BLOB entropyBlob = {};
        if (!entropy.empty()) {
            entropyBlob.pbData = reinterpret_cast<BYTE *>(const_cast<char *>(entropy.data()));
            entropyBlob.cbData = static_cast<DWORD>(entropy.size());
        }

        DATA_BLOB decryptedBlob = {};
        if (!CryptUnprotectData(&encryptedBlob, nullptr, entropy.empty() ? nullptr : &entropyBlob, nullptr, nullptr, 0,
                                &decryptedBlob)) {
            return false;
        }

        outPlaintext.assign(reinterpret_cast<char *>(decryptedBlob.pbData), decryptedBlob.cbData);
        SecureZeroMemory(decryptedBlob.pbData, decryptedBlob.cbData);
        LocalFree(decryptedBlob.pbData);
        return true;
    }

    static std::wstring Utf8ToWide(const std::string &utf8) {
        if (utf8.empty()) {
            return {};
        }
        const int len = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), static_cast<int>(utf8.size()), nullptr, 0);
        if (len <= 0) {
            return {};
        }
        std::wstring wide(len, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), static_cast<int>(utf8.size()), &wide[0], len);
        return wide;
    }

    static std::string WideToUtf8(const std::wstring &wide) {
        if (wide.empty()) {
            return {};
        }
        const int len = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
        if (len <= 0) {
            return {};
        }
        std::string utf8(len, '\0');
        WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), static_cast<int>(wide.size()), &utf8[0], len, nullptr, nullptr);
        return utf8;
    }

    static bool EnsureDirectoryExists(const char *baseDir) {
        if (CreateDirectoryW(Utf8ToWide(baseDir).c_str(), nullptr) != 0) {
            return true;
        }

        const DWORD errorCode = GetLastError();
        return errorCode == ERROR_ALREADY_EXISTS;
    }

    static std::string BuildPath(const char *baseDir, const char *filename, bool forceTxtExtension) {
        std::string fileNameStr(filename);

        if (forceTxtExtension) {
            fileNameStr += ".txt";
        }

        std::string fullPath(baseDir);
        fullPath += "\\";
        fullPath += fileNameStr;
        return fullPath;
    }

    static bool TryGetFullPath(const std::string &inputPath, std::string &outFullPath) {
        const std::wstring wideInput = Utf8ToWide(inputPath);
        wchar_t buffer[MAX_PATH] = {};
        const DWORD result = GetFullPathNameW(wideInput.c_str(), MAX_PATH, buffer, nullptr);
        if (result == 0 || result >= MAX_PATH) {
            return false;
        }

        outFullPath = WideToUtf8(std::wstring(buffer, result));
        return true;
    }

    static bool ResolveValidatedPath(const char *baseDir, const char *filename, bool forceTxtExtension,
                                     std::string &outPath) {
        if (!filename || filename[0] == '\0') {
            return false;
        }

        std::string baseFullPath;
        if (!TryGetFullPath(baseDir, baseFullPath)) {
            return false;
        }

        std::string candidatePath = BuildPath(baseDir, filename, forceTxtExtension);
        std::string candidateFullPath;
        if (!TryGetFullPath(candidatePath, candidateFullPath)) {
            return false;
        }

        std::string basePrefix = baseFullPath;
        if (!basePrefix.empty() && basePrefix.back() != '\\' && basePrefix.back() != '/') {
            basePrefix += "\\";
        }

        if (candidateFullPath.size() <= basePrefix.size()) {
            return false;
        }

        if (_strnicmp(candidateFullPath.c_str(), basePrefix.c_str(), basePrefix.size()) != 0) {
            return false;
        }

        outPath = candidateFullPath;
        return true;
    }

    static uint32_t ReadFileFromDirectory(uintptr_t *luaState, const char *baseDir, bool forceTxtExtension,
                                          bool returnNilIfMissing) {
        const auto *filename = lua_tostring(luaState, 1);
        std::string fullPath;
        if (!ResolveValidatedPath(baseDir, filename, forceTxtExtension, fullPath)) {
            std::string err = "Invalid filename/path (must remain inside ";
            err += baseDir;
            err += ").";
            lua_error(luaState, err.c_str());
            return 0;
        }

        DEBUG_LOG("Attempting to read file from " << baseDir << ": " << fullPath);

        const std::wstring wideFullPath = Utf8ToWide(fullPath);
        const DWORD attributes = GetFileAttributesW(wideFullPath.c_str());
        if (attributes == INVALID_FILE_ATTRIBUTES) {
            const DWORD errorCode = GetLastError();
            if (returnNilIfMissing && (errorCode == ERROR_FILE_NOT_FOUND || errorCode == ERROR_PATH_NOT_FOUND)) {
                lua_pushnil(luaState);
                return 1;
            }
            lua_error(luaState, "Failed to open file for reading.");
            return 0;
        }

        if ((attributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            lua_error(luaState, "Failed to open file for reading.");
            return 0;
        }

        std::ifstream in(wideFullPath);
        if (!in) {
            lua_error(luaState, "Failed to open file for reading.");
            return 0;
        }

        std::ostringstream buf;
        buf << in.rdbuf();
        std::string data = buf.str();

        lua_pushstring(luaState, const_cast<char *>(data.c_str()));
        return 1;
    }

    static uint32_t WriteFileToDirectory(uintptr_t *luaState, const char *baseDir, bool forceTxtExtension,
                                         const char *filename, char mode, const char *content) {
        if (!EnsureDirectoryExists(baseDir)) {
            lua_error(luaState, "Failed to create output directory.");
            return 0;
        }

        std::string fullPath;
        if (!ResolveValidatedPath(baseDir, filename, forceTxtExtension, fullPath)) {
            std::string err = "Invalid filename/path (must remain inside ";
            err += baseDir;
            err += ").";
            lua_error(luaState, err.c_str());
            return 0;
        }

        DEBUG_LOG("Writing file to " << baseDir << ": " << fullPath << " (mode=" << mode << ")");

        std::ios::openmode openMode = std::ios::out;
        if (mode == 'w') {
            openMode |= std::ios::trunc;
        } else if (mode == 'a') {
            openMode |= std::ios::app;
        } else if (mode == 'b') {
            openMode |= std::ios::binary;
        } else {
            lua_error(luaState, "WriteCustomFile: mode must be 'w', 'b', or 'a'");
            return 0;
        }

        std::ofstream out(Utf8ToWide(fullPath), openMode);
        if (!out) {
            lua_error(luaState, "Failed to open file for writing.");
            return 0;
        }

        out << content;
        if (!out) {
            lua_error(luaState, "Failed to write file.");
            return 0;
        }

        return 0;
    }

    uint32_t Script_WriteCustomFile(uintptr_t *luaState) {
        luaState = GetLuaStatePtr();

        const int argCount = lua_gettop(luaState);
        if (argCount < 2 || argCount > 3) {
            lua_error(luaState, "Usage: WriteCustomFile(filename, content, [mode])");
            return 0;
        }

        if (!lua_isstring(luaState, 1) || !lua_isstring(luaState, 2)) {
            lua_error(luaState, "Usage: WriteCustomFile(filename, content, [mode])");
            return 0;
        }

        const auto *filename = lua_tostring(luaState, 1);
        if (!ValidateLuaFilename(luaState, filename)) {
            return 0;
        }

        const auto *content = lua_tostring(luaState, 2);
        char mode = 'w';
        if (argCount == 3) {
            if (!lua_isstring(luaState, 3)) {
                lua_error(luaState, "Usage: WriteCustomFile(filename, content, [mode])");
                return 0;
            }
            const auto *modeStr = lua_tostring(luaState, 3);
            if (!modeStr || modeStr[0] == '\0' || modeStr[1] != '\0') {
                lua_error(luaState, "WriteCustomFile: mode must be 'w', 'b', or 'a'");
                return 0;
            }
            mode = modeStr[0];
        }

        if (!content) {
            lua_error(luaState, "WriteCustomFile: invalid content");
            return 0;
        }

        return WriteFileToDirectory(luaState, "CustomData", false, filename, mode, content);
    }

    uint32_t Script_ReadCustomFile(uintptr_t *luaState) {
        luaState = GetLuaStatePtr();
        if (!lua_isstring(luaState, 1)) {
            lua_error(luaState, "Usage: ReadCustomFile(filename)");
            return 0;
        }

        const auto *filename = lua_tostring(luaState, 1);
        if (!ValidateLuaFilename(luaState, filename)) {
            return 0;
        }
        return ReadFileFromDirectory(luaState, "CustomData", false, true);
    }

    uint32_t Script_CustomFileExists(uintptr_t *luaState) {
        luaState = GetLuaStatePtr();
        if (!lua_isstring(luaState, 1)) {
            lua_error(luaState, "Usage: CustomFileExists(filename)");
            return 0;
        }

        const auto *filename = lua_tostring(luaState, 1);
        if (!ValidateLuaFilename(luaState, filename)) {
            return 0;
        }

        std::string fullPath;
        if (!ResolveValidatedPath("CustomData", filename, false, fullPath)) {
            lua_error(luaState, "Invalid filename/path (must remain inside CustomData).");
            return 0;
        }

        const DWORD attributes = GetFileAttributesW(Utf8ToWide(fullPath).c_str());
        if (attributes == INVALID_FILE_ATTRIBUTES) {
            const DWORD errorCode = GetLastError();
            if (errorCode == ERROR_FILE_NOT_FOUND || errorCode == ERROR_PATH_NOT_FOUND) {
                lua_pushboolean(luaState, 0);
                return 1;
            }
            lua_error(luaState, "CustomFileExists: failed to check file");
            return 0;
        }

        lua_pushboolean(luaState, (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0);
        return 1;
    }

    uint32_t Script_ImportFile(uintptr_t *luaState) {
        luaState = GetLuaStatePtr();
        if (!lua_isstring(luaState, 1)) {
            lua_error(luaState, "Usage: ImportFile(filename)");
            return 0;
        }

        const auto *filename = lua_tostring(luaState, 1);
        if (!ValidateLuaFilename(luaState, filename)) {
            return 0;
        }

        return ReadFileFromDirectory(luaState, "Imports", true, true);
    }

    uint32_t Script_ExportFile(uintptr_t *luaState) {
        luaState = GetLuaStatePtr();
        if (lua_gettop(luaState) != 2 || !lua_isstring(luaState, 1) || !lua_isstring(luaState, 2)) {
            lua_error(luaState, "Usage: ExportFile(filename, text)");
            return 0;
        }

        const auto *filename = lua_tostring(luaState, 1);
        if (!ValidateLuaFilename(luaState, filename)) {
            return 0;
        }

        const auto *content = lua_tostring(luaState, 2);
        lua_settop(luaState, 2);
        return WriteFileToDirectory(luaState, "Imports", true, filename, 'w', content);
    }

    uint32_t Script_ExecuteCustomLuaFile(uintptr_t *luaState) {
        luaState = GetLuaStatePtr();
        if (!lua_isstring(luaState, 1)) {
            lua_error(luaState, "Usage: ExecuteCustomLuaFile(filename)");
            return 0;
        }

        const auto *filename = lua_tostring(luaState, 1);
        if (!ValidateLuaFilename(luaState, filename)) {
            return 0;
        }

        const char *dot = std::strrchr(filename, '.');
        if (!dot || _stricmp(dot, ".lua") != 0) {
            lua_error(luaState, "ExecuteCustomLuaFile: filename must end with .lua");
            return 0;
        }

        std::string validatedFullPath;
        if (!ResolveValidatedPath("CustomData", filename, false, validatedFullPath)) {
            lua_error(luaState, "Invalid filename/path (must remain inside CustomData).");
            return 0;
        }

        std::string relativePath = BuildPath("CustomData", filename, false);
        DEBUG_LOG("Executing Lua file: " << validatedFullPath);
        auto const executeFile = reinterpret_cast<uint32_t(__fastcall *)(char *filePath, int *updateMD5, int *cStatus)>(
            Offsets::FrameScript_ExecuteFile);
        executeFile(const_cast<char *>(relativePath.c_str()), nullptr, nullptr);

        return 0;
    }

    uint32_t Script_EncryptPassword(uintptr_t *luaState) {
        luaState = GetLuaStatePtr();

        if (!lua_isstring(luaState, 1)) {
            lua_error(luaState, "Usage: EncryptPassword(password)");
            return 0;
        }

        auto *password = lua_tostring(luaState, 1);
        if (!password) {
            lua_error(luaState, "EncryptPassword: invalid password");
            return 0;
        }

        const std::string passwordString(password);
        const std::string prefix(kEncryptedPasswordPrefix);
        if (passwordString.compare(0, prefix.size(), prefix) == 0) {
            lua_pushstring(luaState, const_cast<char *>(passwordString.c_str()));
            return 1;
        }

        std::string entropy;
        if (!GetEnvVar("WOW_ENCRYPTION_KEY", entropy)) {
            lua_error(luaState, "EncryptPassword: WOW_ENCRYPTION_KEY is not set");
            return 0;
        }

        std::string encryptedPasswordBase64;
        if (!EncryptDpapiBase64(passwordString, entropy, encryptedPasswordBase64)) {
            lua_error(luaState, "EncryptPassword: failed to encrypt password");
            return 0;
        }

        encryptedPasswordBase64.insert(0, prefix);

        lua_pushstring(luaState, const_cast<char *>(encryptedPasswordBase64.c_str()));
        return 1;
    }

    uint32_t Script_EncryptedServerLogin(uintptr_t *luaState) {
        luaState = GetLuaStatePtr();

        if (!lua_isstring(luaState, 1) || !lua_isstring(luaState, 2)) {
            lua_error(luaState, "Usage: EncryptedServerLogin(username, encryptedPasswordBase64String)");
            return 0;
        }

        auto *username = lua_tostring(luaState, 1);
        auto *encryptedPasswordBase64 = lua_tostring(luaState, 2);
        if (!username || !encryptedPasswordBase64) {
            lua_error(luaState, "EncryptedServerLogin: invalid username or encrypted password");
            return 0;
        }

        std::string encryptedPasswordValue(encryptedPasswordBase64);
        const std::string prefix(kEncryptedPasswordPrefix);
        if (encryptedPasswordValue.compare(0, prefix.size(), prefix) != 0) {
            lua_error(luaState, "EncryptedServerLogin: encrypted password must start with :encrypted:");
            return 0;
        }
        encryptedPasswordValue.erase(0, prefix.size());

        std::string entropy;
        if (!GetEnvVar("WOW_ENCRYPTION_KEY", entropy)) {
            lua_error(luaState, "EncryptedServerLogin: WOW_ENCRYPTION_KEY is not set");
            return 0;
        }

        std::string decryptedPassword;
        if (!DecryptDpapiBase64(encryptedPasswordValue, entropy, decryptedPassword)) {
            lua_error(luaState, "EncryptedServerLogin: failed to decrypt password");
            return 0;
        }

        auto const defaultServerLogin = reinterpret_cast<void(__fastcall *)(char *username, char *password)>(
            Offsets::CGlueMgr_DefaultServerLogin);
        defaultServerLogin(username, const_cast<char *>(decryptedPassword.c_str()));
        return 0;
    }

}
