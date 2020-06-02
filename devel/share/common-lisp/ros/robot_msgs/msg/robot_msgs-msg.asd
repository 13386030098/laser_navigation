
(cl:in-package :asdf)

(defsystem "robot_msgs-msg"
  :depends-on (:roslisp-msg-protocol :roslisp-utils )
  :components ((:file "_package")
    (:file "odometer" :depends-on ("_package_odometer"))
    (:file "_package_odometer" :depends-on ("_package"))
  ))