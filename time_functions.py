# time-related functions
import datetime
import re
import numpy as np


# check if two programs are the same and return response boolean
def isSameProgram(program1, program2):
    sameDays = program1.days == program2.days
    sameTiming = program1.timing == program2.timing
    sameAmount = program1.amount == program2.amount

    return sameDays and sameTiming and sameAmount

def getDayNumber(day):
    if(day == 'Mon'):
        return 0
    elif(day == 'Tue'):
        return 1
    elif(day == 'Wed'):
        return 2
    elif(day == 'Thu'):
        return 3
    elif(day == 'Fri'):
        return 4
    elif(day == 'Sat'):
        return 5
    elif(day == 'Sun'):
        return 6
    else: return -1

# calculate the remaining days until the next watering
'''
in the following we code we'll process the strings that form
the array of days of the app's data structure. The algorithm
is as follows:
1 - get a hold of the current weekday, represented by a number 0-6
2 - decode the payload string (which is bytes format)
3 - use regular expressions (regex) to separate each day in the string
    because it'll come as something like 'Mon Tue Wed', so we put each
    day on our own array of days (match)
4 - find, in this array, which of the days is the closest to the current
    weekday (if we have more than one and some of those days are still
    ahead of us).
    4.1 - if there's only one day, it'll obviously be the next watering
    day.
    4.2 - if there's more than one day and they all have passed already
    then the next day will be the smaller of the days (0-6).
    4.3 - if there's more than one day and all of these days are
    still ahead of us the next watering day is again the smallest
    day number.
    4.4 - if there's more than one day and today is somewhere in between
    those days, chose the next day bigger than the current day.
    4.5 - if one of the days is today we need to check the timing.
'''
def ETANextWateringDay(program):
    today = datetime.datetime.now().weekday() # 0 is monday, 6 is sunday
    print('today is',today)
    # expecting a string such as 'Mon Tue Wed',three letters followed by
    # a whitespace
    days = program['days'].decode('utf-8')
    match = re.findall(r'(\w+)\s*', days)

    # one array to hold the days after we converted them
    # from strings to integers
    watering_days = []


    if match:
        for i in range(len(match)):
            watering_days.append(getDayNumber(match[i]))

    # let's turn it into a numpy array for good measure
    watering_days = np.array(watering_days)
    print(watering_days)

    # only one day case
    if(len(watering_days) == 1 and watering_days[0] > -1):
        return watering_days[0]

    # all days passed or no day passed case
    elif((watering_days.min() > today) or (watering_days.max() < today)):
        return watering_days.min()

    # current day in between the selected days
    elif((today > watering_days.min()) and (today < watering_days.max())):
        for day in watering_days:
            # return the next selected day from today
            if day > today: return day

    # today is one of the days
    elif(len(np.where(watering_days == today)) > 0):
        return today
    # something went wrong
    else:
        return (-1)


# calculate the remaining time until next watering
# returns hours a minutes to deadline if we consider only
# the time constraints
def ETANextWateringTime(program):
    #now = str(datetime.datetime.now())
    now = datetime.datetime.now().time()
    timing = program['timing'].decode('utf-8')
    timing_hour = int(timing[0:2])
    timing_minute = int(timing[3:5])
    # there must be a better way of doing this :D
    now_hour = int(str(now)[0:2])
    now_minute= int(str(now)[3:5])

    hours_to_deadline = 0
    mins_to_deadline = 0

    # flag to tell if the watering time has already passed
    passed = False

    # if we havent passed the deadline hour yet
    if(now_hour <  timing_hour):
        hours_to_deadline = timing_hour - now_hour

    # if we are at the deadline hour
    elif(now_hour == timing_hour):
        hours_to_deadline = 0
        # if we have passed the deadline by a few mins
        if(now_minute > timing_minute): passed = True

    # if we passed the deadline hour
    else:
        hours_to_deadline = 24 - now_hour + timing_hour
        passed = True

    if(now_minute < timing_minute):

        mins_to_deadline = timing_minute - now_minute

    elif(now_minute == timing_minute):

        mins_to_deadline = 0

    else:
        mins_to_deadline = 60 + timing_minute - now_minute
        hours_to_deadline -= 1
    print('deadline:',hours_to_deadline,':',mins_to_deadline)

    return hours_to_deadline, mins_to_deadline, passed

'''
TODO:document this function
'''
def TimeToNextWatering(program):

    hours, mins, time_overdue = ETANextWateringTime(program)
    next_wt_day = ETANextWateringDay(program)

    today = datetime.datetime.now().weekday()

    if(next_wt_day == today):
        # if the only watering day is today but schedule's overdue
        if(time_overdue):
            hours += 6*24 # next watering is next week at this time

    # next day is tomorrow or later
    elif(next_wt_day > today):
        if(time_overdue):
            hours += (next_wt_day - today - 1)*24
        else: hours += (next_wt_day - today)*24

    # next day is somewhere next week
    elif(next_wt_day < today):
        print('next day somewhere next week')
        if(time_overdue): hours += ((7 - today - 1) + next_wt_day)*24
        else : hours += ((7 - today) + next_wt_day)*24

    else: hours = -1 # something went wrong

    return hours, mins
