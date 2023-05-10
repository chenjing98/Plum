import os

def read_file(file_name, log_file_name):
    prevTime = 0.0
    uidRate = {}
    with open(file_name, 'r') as f:
        line = f.readline()
        while line:
            linelist = line.split(" ")
            if linelist[0] == 'Start':
                line = f.readline()
                continue
            time = float(linelist[0])
            uid = float(linelist[1])
            rxRate = float(linelist[2])
            txRate = float(linelist[3])
            if time - prevTime < 600000 :
                # same time period
                if uid not in uidRate:
                    uidRate[uid] = [rxRate, txRate]
                else:
                    uidRate[uid][0] += rxRate
                    uidRate[uid][1] += txRate
            else:
                # log the previous time period
                total_proc = len(uidRate)
                volumetric_proc = 0
                for rates in uidRate.values():
                    if(rates[0] + rates[1] > 100):
                        volumetric_proc += 1
                if total_proc > 0:
                    with open(log_file_name, 'a') as log:
                        log.write(str(prevTime) + " " + str(total_proc) + " " + str(volumetric_proc) + "\n")
                # new time period
                prevTime = time
                uidRate = {}
                uidRate[uid] = [rxRate, txRate]
                
            line = f.readline()
        
        # log the last time period
        total_proc = len(uidRate)
        volumetric_proc = 0
        for rates in uidRate.values():
            if(rates[0] + rates[1] > 100):
                volumetric_proc += 1
        if total_proc > 0:
            with open(log_file_name, 'a') as log:
                log.write(str(prevTime) + " " + str(total_proc) + " " + str(volumetric_proc) + "\n")    
            
def main():
    summary_file = "apptraffic.summary"
    
    log_dir = "./app_traffic_log/"
    file_walk = os.walk(log_dir)
    
    for path, dir_list, file_list in file_walk:
        for file_name in file_list:
            if file_name.endswith(".txt"):
                print("Processing file: " + file_name)
                read_file(os.path.join(path, file_name), summary_file)
    
if __name__ == "__main__":
    main()