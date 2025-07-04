#include <iostream>
#include <vector>
#include <string>
#include <fstream>

#include "../peforge/include/peforge.hpp"

bool is_dll(const BYTE* buffer) {
    IMAGE_FILE_HEADER* file_header = peforge::get_file_header(buffer);
    return file_header && (file_header->Characteristics & IMAGE_FILE_DLL);
}

bool is_exe(const BYTE* buffer) {
    IMAGE_FILE_HEADER* file_header = peforge::get_file_header(buffer);
    return file_header && (file_header->Characteristics & IMAGE_FILE_EXECUTABLE_IMAGE);
}

bool is_safe_to_convert(const BYTE* buffer) {
    // Check if relocations are not empty
    IMAGE_DATA_DIRECTORY* reloc_dir = peforge::get_directory_entry(buffer, IMAGE_DIRECTORY_ENTRY_BASERELOC);
    return reloc_dir && reloc_dir->VirtualAddress && reloc_dir->Size;
}

BYTE* insert_to_code_cave(BYTE* buffer, size_t buffer_size, const BYTE* data, size_t data_size, DWORD characteristics) {
    // Find a suitable code cave
    DWORD cave_size = 0;
    BYTE* code_cave = peforge::get_minimum_cave(buffer, data_size + 1, &cave_size, characteristics);
    if (!code_cave) {
        std::cerr << "Failed to find suitable code cave" << std::endl;
        return nullptr;
    }

    // Copy the data to the code cave, reserve 1 byte as a separator from (possible) previous data
    BYTE* data_location = code_cave + 1; // Skip one byte as separator
    memcpy(data_location, data, data_size);

    return data_location;
}

bool convert_exe_to_dll(BYTE* buffer, size_t size, const std::string& exportName, const std::string& dllName) {
    // Set the DLL flag
    IMAGE_FILE_HEADER* file_header = peforge::get_file_header(buffer);
    if (!file_header) {
        std::cerr << "Failed to get file header" << std::endl;
        return false;
    }
    file_header->Characteristics |= IMAGE_FILE_DLL;

    // Allocate DllMain
    BYTE dll_main_32[] = {
        0xB8, 0x01, 0x00, 0x00, 0x00,   // mov eax, 1
        0xC2, 0x0C, 0x00                // retn 0x0C
    };

    BYTE dll_main_64[] = {
        0xB8, 0x01, 0x00, 0x00, 0x00,   // mov eax, 1
        0xC3                            // retn 
    };

    BYTE *dll_main = nullptr;
    if (peforge::is_64bit(buffer)) {
        dll_main = insert_to_code_cave(buffer, size, dll_main_64, sizeof(dll_main_64), IMAGE_SCN_MEM_EXECUTE);
    } else {
        dll_main = insert_to_code_cave(buffer, size, dll_main_32, sizeof(dll_main_32), IMAGE_SCN_MEM_EXECUTE);
    }
    
    if (!dll_main) {
        std::cerr << "Failed to insert DllMain code" << std::endl;
        return false;
    }

    // Calculate offset from buffer start
    DWORD dll_main_offset = static_cast<DWORD>(dll_main - buffer);

    DWORD dll_main_rva = 0;
    if (!peforge::offset_to_rva(buffer, static_cast<DWORD>(size), dll_main_offset, dll_main_rva)) {
        std::cerr << "Failed to convert DllMain offset to RVA" << std::endl;
        return false;
    }

    // Update the entry point
    DWORD oep = peforge::get_entry_point(buffer);
    
    if (!peforge::update_entry_point(buffer, dll_main_rva)) {
        std::cerr << "Failed to update entry point" << std::endl;
        return false;
    }

    // Insert function name
    BYTE* function_name_cave = insert_to_code_cave(buffer, size, (BYTE*)exportName.c_str(), exportName.size() + 1, IMAGE_SCN_MEM_READ); // +1 for null terminator
    if (!function_name_cave) {
        std::cerr << "Failed to insert function name" << std::endl;
        return false;
    }
    
    // Get function name rva
    DWORD function_name_offset = static_cast<DWORD>(function_name_cave - buffer);
    DWORD function_name_rva = 0;
    if (!peforge::offset_to_rva(buffer, static_cast<DWORD>(size), function_name_offset, function_name_rva)) {
        std::cerr << "Failed to convert function name offset to RVA" << std::endl;
        return false;
    }

    // Insert dll name
    BYTE* dll_main_name_cave = insert_to_code_cave(buffer, size, (BYTE*)dllName.c_str(), dllName.size() + 1, IMAGE_SCN_MEM_READ); // +1 for null terminator
    if (!dll_main_name_cave) {
        std::cerr << "Failed to insert DLL name" << std::endl;
        return false;
    }
    
    // Get dll name rva
    DWORD dll_main_name_offset = static_cast<DWORD>(dll_main_name_cave - buffer);
    DWORD dll_main_name_rva = 0;
    if (!peforge::offset_to_rva(buffer, static_cast<DWORD>(size), dll_main_name_offset, dll_main_name_rva)) {
        std::cerr << "Failed to convert DLL name offset to RVA" << std::endl;
        return false;
    }

    // Insert rva to array of function names
    BYTE* address_of_names_cave = insert_to_code_cave(buffer, size, (BYTE*)&function_name_rva, sizeof(DWORD), IMAGE_SCN_MEM_READ);
    if (!address_of_names_cave) {
        std::cerr << "Failed to insert address of names" << std::endl;
        return false;
    }
    
    // Get address of names rva
    DWORD address_of_names_offset = static_cast<DWORD>(address_of_names_cave - buffer);
    DWORD address_of_names_rva = 0;
    if (!peforge::offset_to_rva(buffer, static_cast<DWORD>(size), address_of_names_offset, address_of_names_rva)) {
        std::cerr << "Failed to convert address of names offset to RVA" << std::endl;
        return false;
    }

    // Insert rva to array of function addresses
    BYTE* address_of_functions_cave = insert_to_code_cave(buffer, size, (BYTE*)&oep, sizeof(DWORD), IMAGE_SCN_MEM_READ);
    if (!address_of_functions_cave) {
        std::cerr << "Failed to insert address of functions" << std::endl;
        return false;
    }
    
    // Get address of functions rva
    DWORD address_of_functions_offset = static_cast<DWORD>(address_of_functions_cave - buffer);
    DWORD address_of_functions_rva = 0;
    if (!peforge::offset_to_rva(buffer, static_cast<DWORD>(size), address_of_functions_offset, address_of_functions_rva)) {
        std::cerr << "Failed to convert address of functions offset to RVA" << std::endl;
        return false;
    }

    // Create ordinal array
    BYTE* ordinals_cave = insert_to_code_cave(buffer, size, (BYTE*)"\x00\x00", 2, IMAGE_SCN_MEM_READ); // Ordinal 0
    if (!ordinals_cave) {
        std::cerr << "Failed to insert ordinals" << std::endl;
        return false;
    }

    // Get ordinals rva
    DWORD ordinals_offset = static_cast<DWORD>(ordinals_cave - buffer);
    DWORD ordinals_rva = 0;
    if (!peforge::offset_to_rva(buffer, static_cast<DWORD>(size), ordinals_offset, ordinals_rva)) {
        std::cerr << "Failed to convert ordinals offset to RVA" << std::endl;
        return false;
    }

    // Create export directory
    IMAGE_EXPORT_DIRECTORY export_dir = {0};
    export_dir.Name = dll_main_name_rva;
    export_dir.Base = 1;
    export_dir.NumberOfFunctions = 1;
    export_dir.NumberOfNames = 1;
    export_dir.AddressOfFunctions = address_of_functions_rva;
    export_dir.AddressOfNames = address_of_names_rva;
    export_dir.AddressOfNameOrdinals = ordinals_rva;

    // Insert export table
    BYTE* export_table_cave = insert_to_code_cave(buffer, size, (BYTE*)&export_dir, sizeof(IMAGE_EXPORT_DIRECTORY), IMAGE_SCN_MEM_READ);
    if (!export_table_cave) {
        std::cerr << "Failed to insert export table" << std::endl;
        return false;
    }
    
    // Get export table rva
    DWORD export_table_offset = static_cast<DWORD>(export_table_cave - buffer);
    DWORD export_table_rva = 0;
    if (!peforge::offset_to_rva(buffer, static_cast<DWORD>(size), export_table_offset, export_table_rva)) {
        std::cerr << "Failed to convert export table offset to RVA" << std::endl;
        return false;
    }

    // Update export directory
    IMAGE_DATA_DIRECTORY* export_dir_entry = peforge::get_directory_entry(buffer, IMAGE_DIRECTORY_ENTRY_EXPORT);
    if (!export_dir_entry) {
        std::cerr << "Failed to get export directory entry" << std::endl;
        return false;
    }
    export_dir_entry->VirtualAddress = export_table_rva;
    // TODO: Calculate the total size of the export table structure
    // The export table size should include: export directory, function addresses array, function names array, and ordinals array
    // However, most PE loaders ignore this size field, so using just the export directory size is sufficient
    export_dir_entry->Size = sizeof(IMAGE_EXPORT_DIRECTORY);

    return true;
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cout << "Usage: " << argv[0] << " <input_exe> <output_dll> <export_name>" << std::endl;
        std::cout << "Example: " << argv[0] << " input.exe output.dll Start" << std::endl;
        return 1;
    }
    
    std::string inputExe = argv[1];
    std::string outputDll = argv[2];
    std::string exportName = argv[3];
    
    // Extract base name from output DLL for the internal DLL name
    std::string dllName = outputDll;
    size_t lastSlash = dllName.find_last_of("/\\");
    if (lastSlash != std::string::npos) {
        dllName = dllName.substr(lastSlash + 1);
    }
    
    std::ifstream file(inputExe, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to open input file: " << inputExe << std::endl;
        return 1;
    }
    
    std::vector<BYTE> buffer(std::istreambuf_iterator<char>(file), {});
    file.close();

    // Check if the file is a valid EXE
    if (!is_exe(buffer.data())) {
        std::cerr << "The file is not a valid EXE" << std::endl;
        return 1;
    }

    // Check if the file is safe to convert
    if (!is_safe_to_convert(buffer.data())) {
        std::cerr << "The file is not safe to convert (missing relocations)" << std::endl;
        return 1;
    }

    // Convert the EXE to DLL
    if (!convert_exe_to_dll(buffer.data(), buffer.size(), exportName, dllName)) {
        std::cerr << "Failed to convert EXE to DLL" << std::endl;
        return 1;
    }

    // Save the DLL
    std::ofstream outputFile(outputDll, std::ios::binary);
    if (!outputFile.is_open()) {
        std::cerr << "Failed to open output file: " << outputDll << std::endl;
        return 1;
    }
    outputFile.write(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(buffer.size()));
    outputFile.close();
    std::cout << "Successfully converted EXE to DLL: " << outputDll << std::endl;

    return 0;
} 