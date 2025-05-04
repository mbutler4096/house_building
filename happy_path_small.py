import subprocess
import time


#4 3 2
#100 1 10 0
#0 10 13 100
#7 15 18 40

#4 3 3
#100 1 10 0
#0 10 13 100
#7 15 18 40

#5 5 3
#1 1 1 1 1
#1 1 1 1 1
#1 1 1 1 1
#1 1 1 1 1
#1 1 1 1 1


dump_output = 0
executable = './house_building'
stdin_input = [b'4 3 12\n100 1 10 0\n0 10 13 100\n7 15 18 40',
                                 b'4 3 2\n100 1 10 0\n0 10 13 100\n7 15 18 40',
                                 b'5 5 3\n1 1 1 1 1\n1 1 1 1 1\n1 1 1 1 1\n1 1 1 1 1\n1 1 1 1 1',
                                 b'4 3 3\n100 1 10 0\n0 10 13 100\n7 15 18 40']
run = 0;

while(run < len(stdin_input)):
    print("Run: " + str(run+1));

    count = 0

    params = list(map(int, stdin_input[run].strip().split()))

    start_time = time.time()
    if dump_output:
        process = subprocess.Popen([executable], stdin=subprocess.PIPE)
    else:
        process = subprocess.Popen([executable], stdin=subprocess.PIPE, stdout=subprocess.PIPE)
    process.stdin.write(stdin_input[run])
    process.stdin.close()

    if not dump_output:
        table = []
        for line in process.stdout:
            values = list(map(int, line.strip().split()))
            table.append(values)
            count += 1
    process.wait()
    end_time = time.time()

    if not dump_output:
        for row in table:
            print(row)
        print(count)

        if count != params[2]:
            print("Incorrent number of records returned")
            exit(0)

        for row in table:
                if row[0] < 1 or row[0] > params[0]:
                    print("Invalid data found")
                    exit(0)
                if row[1] < 1 or row[1] > params[1]:
                    print("Invalid data found")
                    exit(0)

    run_time = end_time - start_time
    print(f"Run time: {run_time} seconds")

    run += 1;
    print();
