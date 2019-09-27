import sys
import time_functions as tm

current_program = {'timing':b'12:13:10', 'days':b'Mon Tue Fri', 'amount':b'1200'}


def main():
    tm.ETANextWateringTime(current_program)
    nd = tm.ETANextWateringDay(current_program)
    print('next watering day:',nd)
    h,m = tm.TimeToNextWatering(current_program)
    print('absolute time until next watering: ',h,':',m)
if __name__ == '__main__':
    main()
