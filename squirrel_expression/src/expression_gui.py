#!/usr/bin/python
#
# GUI to control stuff on the SQUIRREL robot
# author: Michael Zillich
# date: March 2017

import sys
from PyQt4 import QtCore, QtGui, uic
import rospy
import std_msgs
from squirrel_interaction.srv import DoorController
#.msg import String
#from std_msgs.msg import Float64
#from std_msgs.msg import Bool

form_class = uic.loadUiType("expression_gui.ui")[0]
 
class MyWindowClass(QtGui.QMainWindow, form_class):
    expr_pub = None
    tilt_pub = None
    wiggle_pub = None

    def __init__(self, parent=None):
        QtGui.QMainWindow.__init__(self, parent)
        self.setupUi(self)

        # sound out
        self.helloButton.clicked.connect(self.helloButton_clicked)
        self.cheerfulButton.clicked.connect(self.cheerfulButton_clicked)
        self.noButton.clicked.connect(self.noButton_clicked)
        self.confusedButton.clicked.connect(self.confusedButton_clicked)
        self.thinkButton.clicked.connect(self.thinkButton_clicked)
        # camera tilt movement
        self.upButton.clicked.connect(self.upButton_clicked)
        self.straightButton.clicked.connect(self.straightButton_clicked)
        self.downButton.clicked.connect(self.downButton_clicked)
        # camera pan movement
        self.neckleftButton.clicked.connect(self.neckleftButton_clicked)
        self.neckcenterButton.clicked.connect(self.neckcenterButton_clicked)
        self.neckrightButton.clicked.connect(self.neckrightButton_clicked)
        # head pan movement
        self.headleftButton.clicked.connect(self.headleftButton_clicked)
        self.headcenterButton.clicked.connect(self.headcenterButton_clicked)
        self.headrightButton.clicked.connect(self.headrightButton_clicked)
        # base wiggling
        self.wiggleNoButton.clicked.connect(self.wiggleNoButton_clicked)
        self.wiggleErrorButton.clicked.connect(self.wiggleErrorButton_clicked)
        # door
        self.doorOpenButton.clicked.connect(self.doorOpenButton_clicked)
        self.doorCloseButton.clicked.connect(self.doorCloseButton_clicked)
        # safety reset
        self.safetyResetButton.clicked.connect(self.safetyResetButton_clicked)
        # LEDs
        self.redLEDButton.clicked.connect(self.redLEDButton_clicked)
        self.greenLEDButton.clicked.connect(self.greenLEDButton_clicked)
        self.blueLEDButton.clicked.connect(self.blueLEDButton_clicked)
        self.whiteLEDButton.clicked.connect(self.whiteLEDButton_clicked)
        self.offLEDButton.clicked.connect(self.offLEDButton_clicked)

        rospy.init_node('expression_gui')
        self.led_pub = rospy.Publisher('/light/command', std_msgs.msg.ColorRGBA, queue_size=10)
        self.expr_pub = rospy.Publisher('/expression', std_msgs.msg.String, queue_size=10)
        self.pan_pub = rospy.Publisher('/neck_pan_controller/command', std_msgs.msg.Float64, queue_size=10)
        self.tilt_pub = rospy.Publisher('/neck_tilt_controller/command', std_msgs.msg.Float64, queue_size=10)
        self.head_pub = rospy.Publisher('/head_controller/command', std_msgs.msg.Float64, queue_size=10)
        self.wiggle_pub = rospy.Publisher('/motion_expression', std_msgs.msg.String, queue_size=10)
        self.reset_safety_pub = rospy.Publisher('/reset_safety', std_msgs.msg.Bool, queue_size=10)
        self.arm_mode_pub = rospy.Publisher('/real/robotino/settings/switch_mode', std_msgs.msg.Int32, queue_size=10, latch=True)
        self.door = rospy.ServiceProxy('/door_controller/command', DoorController)

    def helloButton_clicked(self):
        print "Hello"
        msg = std_msgs.msg.String()
        msg.data = "HELLO"
        self.expr_pub.publish(msg)

    def cheerfulButton_clicked(self):
        print "Cheerful"
        msg = std_msgs.msg.String()
        msg.data = "OK"
        self.expr_pub.publish(msg)

    def noButton_clicked(self):
        print "No"
        msg = std_msgs.msg.String()
        msg.data = "NO"
        self.expr_pub.publish(msg)

    def thinkButton_clicked(self):
        print "Think"
        msg = std_msgs.msg.String()
        msg.data = "THINKING"
        self.expr_pub.publish(msg)

    def confusedButton_clicked(self):
        print "Confused"
        msg = std_msgs.msg.String()
        msg.data = "CONFUSED"
        self.expr_pub.publish(msg)

    def upButton_clicked(self):
        print "camera up"
        msg = std_msgs.msg.Float64()
        msg.data = 0.5
        self.tilt_pub.publish(msg)

    def straightButton_clicked(self):
        print "camera straight"
        msg = std_msgs.msg.Float64()
        msg.data = 0.0
        self.tilt_pub.publish(msg)

    def downButton_clicked(self):
        print "camera down"
        msg = std_msgs.msg.Float64()
        msg.data = -0.7
        self.tilt_pub.publish(msg)

    def neckleftButton_clicked(self):
        print "camera left"
        msg = std_msgs.msg.Float64()
        msg.data = -1.0
        if self.checkBox.isChecked():
            self.head_pub.publish(msg)
        self.pan_pub.publish(msg)

    def neckcenterButton_clicked(self):
        print "camera center"
        msg = std_msgs.msg.Float64()
        msg.data = 0.0
        if self.checkBox.isChecked():
            self.head_pub.publish(msg)
        self.pan_pub.publish(msg)

    def neckrightButton_clicked(self):
        print "camera right"
        msg = std_msgs.msg.Float64()
        msg.data = 1.0
        if self.checkBox.isChecked():
            self.head_pub.publish(msg)
        self.pan_pub.publish(msg)

    def headleftButton_clicked(self):
        print "head left"
        msg = std_msgs.msg.Float64()
        msg.data = -1.0
        if self.checkBox.isChecked():
            self.pan_pub.publish(msg)
        self.head_pub.publish(msg)

    def headcenterButton_clicked(self):
        print "head center"
        msg = std_msgs.msg.Float64()
        msg.data = 0.0
        if self.checkBox.isChecked():
            self.pan_pub.publish(msg)
        self.head_pub.publish(msg)

    def headrightButton_clicked(self):
        print "head right"
        msg = std_msgs.msg.Float64()
        msg.data = 1.0
        if self.checkBox.isChecked():
            self.pan_pub.publish(msg)
        self.head_pub.publish(msg)

    def wiggleNoButton_clicked(self):
        print "wiggle no"
        msg = std_msgs.msg.String()
        msg.data = "no"
        self.wiggle_pub.publish(msg)

    def wiggleErrorButton_clicked(self):
        print "wiggle error"
        msg = std_msgs.msg.String()
        msg.data = "error"
        self.wiggle_pub.publish(msg)

    def doorOpenButton_clicked(self):
        print "Open door"
        rospy.wait_for_service('/door_controller/command')
        try:
            resp = self.door('open')
            return resp.result
        except rospy.ServiceException, e:
            rospy.loginfo("Service call failed: {}".format(e))

    def doorCloseButton_clicked(self):
        print "Close door"
        rospy.wait_for_service('/door_controller/command')
        try:
            resp = self.door('close')
            return resp.result
        except rospy.ServiceException, e:
            rospy.loginfo("Service call failed: {}".format(e))

    def safetyResetButton_clicked(self):
        print "reset safety"
        # reset the wrist safety node
        msg = std_msgs.msg.Bool
        msg.data = True
        self.reset_safety_pub.publish(msg)
        # reset the robot controller (works by switching the mode)
        # self.mode_pub.publish(data=0)

    def redLEDButton_clicked(self):
        print "set LEDs to red"
        msg = std_msgs.msg.ColorRGBA(255.0, 0.0, 0.0, 0.0)
        self.led_pub.publish(msg)

    def whiteLEDButton_clicked(self):
        print "set LEDs to white"
        msg = std_msgs.msg.ColorRGBA(255.0, 255.0, 255.0, 0.0)
        self.led_pub.publish(msg)

    def blueLEDButton_clicked(self):
        print "set LEDs to blue"
        msg = std_msgs.msg.ColorRGBA(0.0, 0.0, 255.0, 0.0)
        self.led_pub.publish(msg)

    def greenLEDButton_clicked(self):
        print "set LEDs to green"
        msg = std_msgs.msg.ColorRGBA(0.0, 255.0, 0.0, 0.0)
        self.led_pub.publish(msg)

    def offLEDButton_clicked(self):
        print "set LEDs to off"
        msg = std_msgs.msg.ColorRGBA(0.0, 0.0, 0.0, 0.0)
        self.led_pub.publish(msg)

app = QtGui.QApplication(sys.argv)
myWindow = MyWindowClass(None)
myWindow.show()
app.exec_()

