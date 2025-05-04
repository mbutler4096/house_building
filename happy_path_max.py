import subprocess
import time


dump_output = 0
executable = './house_building'

files = ['max_stepwise.txt', 'max_diagonal.txt', 'max_array.txt', 'max_array2.txt', 'max_array3.txt']
run = 0;

while(run < len(files)):
    print("Run: " + str(run+1));
    string = ''
    params = []

    with open(files[run], 'r') as f:
        count = 0
        for line in f:
            string += line
            if count == 0:
                values = list(map(int, line.strip().split()))
                params.append(values)
            count += 1;

    str_bytes = bytes(string, "utf-8")
    count = 0

    start_time = time.time()
    if dump_output:
        process = subprocess.Popen([executable], stdin=subprocess.PIPE)
    else:
        process = subprocess.Popen([executable], stdin=subprocess.PIPE, stdout=subprocess.PIPE)
    process.stdin.write(str_bytes);
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

        if count != params[0][2]:
            print("Incorrent number of records returned")
            exit(0)

        for row in table:
                if row[0] < 1 or row[0] > params[0][0]:
                    print("Invalid data found")
                    exit(0)
                if row[1] < 1 or row[1] > params[0][1]:
                    print("Invalid data found")
                    exit(0)

        for row in params:
            print(row)

    run_time = end_time - start_time
    print(f"Run time: {run_time} seconds")

    run += 1;
    print();
