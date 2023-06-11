#include <windows.h>
#include <stdio.h>

int main()
{
    HANDLE hFile;
    DWORD dwBytesWritten;
    const char* message = "Hello, World!";
    const char* fileName = "C:\\Users\\ISE\\Desktop\\output.txt";

    hFile = CreateFileA(fileName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

    if (hFile == INVALID_HANDLE_VALUE)
    {
        printf("Error creating file. Error code: %d\n", GetLastError());
        return 1;
    }

    if (!WriteFile(hFile, message, strlen(message), &dwBytesWritten, NULL))
    {
        printf("Error writing to file. Error code: %d\n", GetLastError());
        CloseHandle(hFile);
        return 1;
    }

    printf("Data written to file successfully!\n");
    CloseHandle(hFile);
    while (1) {
        Sleep(1); // Sleep for 1 second
    }
    return 0;
}







