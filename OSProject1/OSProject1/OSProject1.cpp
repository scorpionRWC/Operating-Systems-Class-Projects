#include <fstream>
#include <iostream>
#include <string>
#include <ctime>
#include <cctype>
#include <iomanip>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
using namespace std;

class OS
{
private:
    bool isUser = true;
    int memory[2000];

    //registers
    int LA = 0, PC = 0, SP = 999, IR = 0, AC = 0, X = 0, Y = 0;
    
    //for reading input file
    string line;
    char dot;

    //for timer to know when to switch to kernal
    clock_t timer;
    int TIMER;

    //for forks and pipes
    int pd[2];
    int frk;
public:
    int readData(int);
    void writeData(int, int);
    void initMemory(string, int); // initialize memory array
    void runProgram();
};

int OS::readData(int address) // returns value of given memory address
{
    if(!(isUser && address >= 1000))
        return memory[address];
    else
    {
        cout << "ERROR: User cannot access system address " << address << "...\n";
        system("pause");
    }
}

void OS::writeData(int address, int data) // writes given data to given address
{
    if (!(isUser && address >= 1000))
        memory[address] = data;
    else
    {
        cout << "ERROR: User cannot access system address " << address << "...\n";
        system("pause");
    }
}

void OS::initMemory(string fileName, int t)
{
    TIMER = t; // set the time between process switches
    pipe(pd); // make a pipe
    timer = clock();
    //cout << timer << endl << endl;
    ifstream inputFile(fileName);
    if (inputFile.is_open())
    {
        streamoff p = inputFile.tellg(); // used for backtracking
        inputFile >> skipws; // ignore whitespace
        while (getline(inputFile, line))
        {
            //cout << line << endl;
            if (!line.empty() && (isdigit(line.at(0)) || line.at(0) == '.'))
            {
                if (line.at(0) == '.')
                { // line starting with a dot means to jump addresses
                    inputFile.seekg(p);
                    inputFile >> dot >> LA;
                    getline(inputFile, line);
                }
                else
                {// otherwise just insert data into current address
                    inputFile.seekg(p);
                    inputFile >> IR;
                    getline(inputFile, line);
                    //cout << setw(5) << LA << " : " << IR << endl;
                    memory[LA++] = IR;
                }
            }
            p = inputFile.tellg();
        }
        frk = fork(); // create process
        runProgram();
    }
    else cout << "Unable to open file.\n";
}

void OS::runProgram()
{
    do
    {
        if ((clock() - timer) >= TIMER && isUser == true)
        { //jump to kernal, save SP, AC, and PC
            timer = clock();
            isUser = false;
            if (frk == 0)
                write(pd[1], &SP, sizeof(int));
            else
            {
                read(pd[0], &SP, sizeof(int));
                writeData(1999, SP);
                SP = 1999;
                SP--;
            }
            if (frk == 0)
            {
                --PC;
                write(pd[1], &PC, sizeof(int));
            }
            else
            {
                read(pd[0], &PC, sizeof(int));
                writeData(SP--, PC);
            }
            if (frk == 0)
                write(pd[1], &AC, sizeof(int));
            else
            {
                read(pd[0], &AC, sizeof(int));
                writeData(SP--, AC);
                PC = 1000; // no auto-iteration
            }
        }
        IR = readData(PC);
        switch (IR)
        {
        case 1: // loadValue
            AC = readData(++PC);
            break;
        case 2: // loadAddr
            AC = readData(readData(++PC));
            break;
        case 3: // loadIndAddr
            AC = readData(readData(readData(++PC)));
            break;
        case 4: // loadIndXAddr
            AC = readData(readData(++PC) + X);
            break;
        case 5: // loadIndYAddr
            AC = readData(readData(++PC) + Y);
            break;
        case 6: // loadSpX
            AC = readData(SP + X + 1);
            break;
        case 7: // storeAdr
            writeData(readData(++PC), AC);
            break;
        case 8: // getRandom
            srand((unsigned int)time(NULL));
            AC = rand() % 100 + 1;
            break;
        case 9: // putPort
            if(readData(++PC) == 1)
                cout << AC;
            else if(readData(PC) == 2)
                cout << (char)AC;
            break;
        case 10: // addX
            AC += X;
            break;
        case 11: // addY
            AC += Y;
            break;
        case 12: // subX
            AC -= X;
            break;
        case 13: // subY
            AC -= Y;
            break;
        case 14: // copyToX
            X = AC;
            break;
        case 15: // copyFromX
            AC = X;
            break;
        case 16: // copyToY
            Y = AC;
            break;
        case 17: // copyFromY
            AC = Y;
            break;
        case 18: // copyToSp
            SP = AC;
            break;
        case 19: // copyFromSp
            AC = SP;
            break;
        case 20: // jumpAddr
            PC = readData(++PC) - 1; // because of auto-iteration
            break;
        case 21: // jumpIfEqualAddr
            if (AC == 0)
                PC = readData(++PC) - 1; // because of auto-iteration
            else
                ++PC;
            break;
        case 22: // jumpIfNotEqualAddr
            if (AC != 0)
                PC = readData(++PC) - 1; // because of auto-iteration
            else
                ++PC;
            break;
        case 23: // callAddr
            writeData(SP--, PC + 2);
            PC = readData(PC + 1) - 1; // because of auto-iteration
            break;
        case 24: // ret
            PC = readData(++SP) - 1; // because of auto-iteration
            writeData(SP, NULL);
            break;
        case 25: // incX
            X++;
            break;
        case 26: // decX
            X--;
            break;
        case 27: // push
            writeData(SP--, AC);
            break;
        case 28: // pop
            AC = readData(++SP);
            writeData(SP, NULL);
            break;
        case 29: // int
            if (isUser == true)
            { //jump to kernal, save SP, AC, and PC
                isUser = false;
                if (frk == 0)
                    write(pd[1], &SP, sizeof(int));
                else
                {
                    read(pd[0], &SP, sizeof(int));
                    writeData(1999, SP);
                    SP = 1999;
                    SP--;
                }
                if (frk == 0)
                {
                    --PC;
                    write(pd[1], &PC, sizeof(int));
                }
                else
                {
                    read(pd[0], &PC, sizeof(int));
                    writeData(SP--, PC);
                }
                if (frk == 0)
                    write(pd[1], &AC, sizeof(int));
                else
                {
                    read(pd[0], &AC, sizeof(int));
                    writeData(SP--, AC);
                    PC = 1500 - 1; // because of auto-iteration
                }
            }
            break;
        case 30: // iRet
            if (isUser == false && frk > 0)
            {
                AC = readData(1997);
                PC = readData(1998);
                SP = readData(1999);
                isUser = true;
            }
            break;
        case 50: // stop processes
            if (frk == 0)
                _exit(0);
            else
            {
                waitpid(-1, NULL, 0);
            }
            break;
        default: // stop processes
            if (frk == 0)
                _exit(0);
            else
            {
                waitpid(-1, NULL, 0);
            }
            break;
        }
        PC++;
    }while (IR != 50);
}

int main(string fName, int tmr)
{
    cout << "Start of program!\n\n";
    OS *os = new OS();
    os->initMemory(fName, tmr);
    system("pause");
    return 0;
}