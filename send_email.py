from socket import *
import base64
from email.mime.multipart import MIMEMultipart
from email.mime.text import MIMEText
from email.mime.image import MIMEImage
from email.header import Header
import ssl

subject = "I love computer networks!"
contenttype = "text/plain"
msg = "\r\n I love computer networks!"
img = "./email.jfif"
endmsg = "\r\n.\r\n"
# Choose a mail server (e.g. Google mail server)
# and call it mailserver
mailserver = "smtp.qq.com"#Fill in start #Fill in end
# Sender and reciever
# Fill in start
fromaddress = "782272741@qq.com"
toaddress = "chennina202012@163.com"
# Fill in end

# Auth information (Encode with base64)
# Fill in start
username = base64.b64encode(b'782272741@qq.com')
password = base64.b64encode(b'qokahjlxfewdbeag')
# Fill in end

# Create socket called clientSocket
# and establish a TCP connection with mailserver
#Fill in start
fsock = socket(AF_INET, SOCK_STREAM)
clientSocket = ssl.wrap_socket(fsock, ssl_version=ssl.PROTOCOL_TLSv1)
clientSocket.connect((mailserver, 465))
#Fill in end
recv = clientSocket.recv(1024).decode()
print(recv)
if recv[:3] != '220':
    print('220 reply not received from server.')

# Send HELO command and print server response.
heloCommand = 'HELO Alice\r\n'
clientSocket.send(heloCommand.encode())
recv1 = clientSocket.recv(1024).decode()
print(recv1)
if recv1[:3] != '250':
    print('250 reply not received from server.')

clientSocket.sendall('AUTH LOGIN\r\n'.encode())
recv = clientSocket.recv(1024).decode()
print(recv)
if (recv[:3] != '334'):
	print('334 reply not received from server')

clientSocket.sendall((username + b'\r\n'))
recv = clientSocket.recv(1024).decode()
print(recv)
if (recv[:3] != '334'):
	print('334 reply not received from server')

clientSocket.sendall((password + b'\r\n'))
recv = clientSocket.recv(1024).decode()
print(recv)
if (recv[:3] != '235'):
	print('235 reply not received from server')

# Auth
# Send MAIL FROM command and print server response.
# Fill in start
clientSocket.sendall(('MAIL FROM: <' + fromaddress + '>\r\n').encode())
recv = clientSocket.recv(1024).decode()
print(recv)
if (recv[:3] != '250'):
	print('250 reply not received from server')
# Fill in end

# Send RCPT TO command and print server response.
# Fill in start
clientSocket.sendall(('RCPT TO: <' + toaddress + '>\r\n').encode())
recv = clientSocket.recv(1024).decode()
print(recv)
if (recv[:3] != '250'):
	print('250 reply not received from server')
# Fill in end

# Send DATA command and print server response.
# Fill in start
clientSocket.send('DATA\r\n'.encode())
recv = clientSocket.recv(1024).decode()
print(recv)
if (recv[:3] != '354'):
	print('354 reply not received from server')
# Fill in end

# Send message data.
# Fill in start
msgRoot = MIMEMultipart('related')
msgRoot['From'] = Header(fromaddress)
msgRoot['To'] = Header(toaddress)
msgRoot['Subject'] = Header(subject, 'utf-8')

mail_msg = msg
mail_msg += """
<p>图片：</p>
<p><img src="cid:image1"></p>
"""
msgRoot.attach(MIMEText(mail_msg, 'html', 'utf-8'))

fp = open('email.jfif', 'rb')
msgImage = MIMEImage(fp.read())
# 定义图片 ID，在 HTML 文本中引用
msgImage.add_header('Content-ID', '<image1>')
msgRoot.attach(msgImage)
clientSocket.sendall(bytes(msgRoot))
# Fill in end

# Message ends with a single period.
# Fill in start
clientSocket.sendall(endmsg.encode())
recv = clientSocket.recv(1024).decode()
print(recv)
if (recv[:3] != '250'):
	print('250 reply not received from server')
# Fill in end

# Send QUIT command and get server response.
# Fill in start
clientSocket.sendall('QUIT\r\n'.encode())
# Fill in end
# Close connection
# Fill in start
clientSocket.close()
# Fill in end