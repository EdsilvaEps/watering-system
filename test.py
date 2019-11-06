import sys
import time_functions as tm
import datetime

current_program = {'timing':'09:33:10', 'days':'Mon Tue Fri Sat', 'amount':'1200'}
new_program = {'timing':'12:00:00', 'days':'Mon Wed Fri Sun ', 'amount':'1200'}
#system/app/days b'Mon Wed Fri Sun '
# system/app/timing b'12:00:00'

def main():
    print('current time: ', datetime.datetime.now().time())
    #tm.ETANextWateringTime(current_program)
    print('new prog is same: ', tm.isSameProgram(current_program, new_program))
    if(not tm.isSameProgram(current_program, new_program)):
        current_program['timing'] = new_program['timing']
        current_program['days'] = new_program['days']
        current_program['amount'] = new_program['amount']

        nd = tm.ETANextWateringDay(current_program)
        print('next watering day:',nd)
        h,m = tm.TimeToNextWatering(current_program)
        print('absolute time until next watering: ',h,':',m)


if __name__ == '__main__':
    main()
