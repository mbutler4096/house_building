import subprocess
import time

executable = './house_building'
stdin_input = [b'4 3 3 3\n100 1 10 0\n0 10 13 100\n7 15 18 40',
                                  b'4 3\n100 1 10 0\n0 10 13 100\n7 15 18 40',
                                  b'4\n100 1 10 0\n0 10 13 100\n7 15 18 40',
                                  b'\n100 1 10 0\n0 10 13 100\n7 15 18 40',
                                  b'10000 3 3\n100 1 10 0\n0 10 13 100\n7 15 18 40',
                                  b'4 10000 3\n100 1 10 0\n0 10 13 100\n7 15 18 40',
                                  b'4 3 100000\n100 1 10 0\n0 10 13 100\n7 15 18 40',
                                  b'A 3 3\n100 1 10 0\n0 10 13 100\n7 15 18 40',
                                  b'4 A 3\n100 1 10 0\n0 10 13 100\n7 15 18 40',
                                  b'4 3 A\n100 1 10 0\n0 10 13 100\n7 15 18 40',
                                  b'-4 3 3\n100 1 10 0\n0 10 13 100\n7 15 18 40',
                                  b'4 -3 3\n100 1 10 0\n0 10 13 100\n7 15 18 40',
                                  b'4 3 -3\n100 1 10 0\n0 10 13 100\n7 15 18 40',
                                  b'4 3 3\n100 1 10 0 100\n0 10 13 100 100\n7 15 18 40 100',
                                  b'4 3 3\n1000 1 10 0\n0 10 13 100\n7 15 18 40',
                                  b'4 3 3\n-100 1 10 0\n0 10 13 100\n7 15 18 40',
                                  b'4 3 3\n-100 1 10 0\n0 10 13 100',
                                  b'']
run = 0;

while(run < len(stdin_input)):
    print("run: " + str(run+1));
    process = subprocess.Popen([executable], stdin=subprocess.PIPE)
    process.stdin.write(stdin_input[run])
    process.stdin.close()

    time.sleep(1)
    run += 1;
    print();
