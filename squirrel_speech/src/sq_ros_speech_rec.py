#!/usr/bin/env python

## Simple talker demo that published std_msgs/Strings messages
## to the 'chatter' topic
from __future__ import print_function

import rospy
import speech_recognition as sr
from std_msgs.msg import String

r = sr.Recognizer()
m = sr.Microphone()


print("SQUIRREL SPEECH RECOGNITION -----------------------------------------------")
print("Deutsch") 
print("Google API") 
print(" ") 
print("---------------------------------------------------------------------------") 


def recognizer():
    print("Initialize...")

    pub = rospy.Publisher('topic_rec_speech', String, queue_size=5)  
    rospy.init_node('speech_recognizer_node', anonymous=True)

    with m as source:
        r.adjust_for_ambient_noise(source)
        print("Set minimum energy threshold to {}".format(r.energy_threshold))
        print("---------------------------------------------------------------------------") 

        
        # LOOP -----------------------------------------------------------------------
        while not rospy.is_shutdown():

            try:
                print("Ready!")
                audio = r.listen(source)
                print("Recognition...")
            
                #value_str = r.recognize_google(audio,None,"de")
                value = r.recognize_google(audio,None,"de")
                value = value.encode("utf-8")
                #value = unicode(value_str,"utf-8")
                #value = value.encode("utf-8")
                value = value.lower()

                print("\033[0;32m", end="")  #green
                rospy.loginfo(value)
                print("\033[0;37m", end="")    #white

                pub.publish(value)


            except sr.UnknownValueError:
                print("\033[0;31m", end="")  #red
                print("ERROR: Unrecognizable", end="")
                print("\033[0;37m")   #white
                


            except sr.RequestError as e:
                print("\033[0;31m", end="")  #red
                print("ERROR: Couldn't request results from Google Speech")
                print("Recognition service - {0}".format(e))
                print("\033[0;37m")   #white



if __name__ == '__main__':
    try:
        recognizer()
    except rospy.ROSInterruptException:
        pass


