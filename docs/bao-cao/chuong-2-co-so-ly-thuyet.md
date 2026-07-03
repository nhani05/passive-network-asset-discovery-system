# CHƯƠNG 2. CƠ SỞ LÝ THUYẾT

## 2.1. Tổng quan về phát hiện tài sản mạng

Tài sản mạng là các thiết bị, hệ thống hoặc thành phần có khả năng kết nối và trao đổi dữ liệu trong một hệ thống mạng. Các tài sản này có thể bao gồm máy tính cá nhân, máy chủ, điện thoại thông minh, máy in, camera IP, thiết bị IoT, router, switch, firewall hoặc các thiết bị chuyên dụng khác.

Trong quản trị mạng và an toàn thông tin, việc phát hiện tài sản mạng đóng vai trò rất quan trọng. Quản trị viên cần biết trong mạng đang tồn tại những thiết bị nào, thiết bị đó sử dụng địa chỉ IP nào, địa chỉ MAC là gì, tên thiết bị là gì và thiết bị xuất hiện trong mạng vào thời điểm nào. Những thông tin này giúp hỗ trợ kiểm kê, giám sát, phát hiện thiết bị lạ và đánh giá rủi ro bảo mật.

Nếu không có danh sách tài sản mạng đầy đủ, tổ chức có thể bỏ sót các thiết bị không được quản lý. Những thiết bị này có thể trở thành điểm yếu trong hệ thống, đặc biệt nếu chúng sử dụng mật khẩu mặc định, phần mềm lỗi thời hoặc không được cập nhật bản vá bảo mật.

**[PLACEHOLDER HÌNH 2.1: Chèn sơ đồ minh họa các loại tài sản mạng trong một mạng nội bộ, gồm máy tính, máy chủ, điện thoại, camera IP, máy in, router, switch và thiết bị IoT]**

**Hình 2.1. Minh họa các loại tài sản trong hệ thống mạng nội bộ**

## 2.2. Phát hiện tài sản mạng chủ động và thụ động

Có hai hướng tiếp cận phổ biến trong phát hiện tài sản mạng: phát hiện chủ động và phát hiện thụ động.

Phát hiện chủ động là phương pháp trong đó hệ thống gửi các gói tin thăm dò đến các địa chỉ hoặc thiết bị trong mạng để kiểm tra phản hồi. Ví dụ, công cụ quét mạng có thể gửi gói ICMP Echo Request, ARP Request, TCP SYN hoặc UDP probe để xác định thiết bị nào đang hoạt động.

Ưu điểm của phương pháp chủ động là có thể phát hiện nhanh các thiết bị trong một dải mạng nhất định. Tuy nhiên, phương pháp này cũng có một số hạn chế như tạo thêm lưu lượng mạng, có thể bị tường lửa chặn, gây cảnh báo từ hệ thống giám sát hoặc ảnh hưởng đến các thiết bị nhạy cảm.

Phát hiện thụ động là phương pháp không gửi gói tin thăm dò đến thiết bị. Thay vào đó, hệ thống chỉ lắng nghe, thu thập và phân tích lưu lượng mạng sẵn có. Khi một thiết bị giao tiếp trong mạng, các gói tin mà thiết bị gửi đi có thể chứa thông tin nhận diện như địa chỉ IP, địa chỉ MAC, hostname hoặc tên dịch vụ. Từ đó, hệ thống có thể tổng hợp thông tin để xây dựng danh sách tài sản mạng.

**[PLACEHOLDER HÌNH 2.2: Chèn sơ đồ so sánh Active Discovery và Passive Discovery. Active Discovery có mũi tên gửi probe đến thiết bị; Passive Discovery chỉ lắng nghe lưu lượng mạng]**

**Hình 2.2. So sánh phát hiện chủ động và phát hiện thụ động**

Bảng dưới đây tóm tắt một số điểm khác nhau giữa hai phương pháp:

| Tiêu chí | Phát hiện chủ động | Phát hiện thụ động |
|---|---|---|
| Cách hoạt động | Gửi gói tin thăm dò | Lắng nghe lưu lượng có sẵn |
| Tác động đến mạng | Có tạo thêm lưu lượng | Ít tác động đến mạng |
| Khả năng phát hiện thiết bị im lặng | Có thể phát hiện nếu thiết bị phản hồi | Khó phát hiện nếu thiết bị không giao tiếp |
| Rủi ro gây cảnh báo | Cao hơn | Thấp hơn |
| Phù hợp với | Kiểm kê nhanh, quét định kỳ | Giám sát liên tục, ít xâm lấn |

Trong đề tài này, hệ thống sử dụng phương pháp phát hiện thụ động nhằm giảm tác động đến mạng và phù hợp với mục tiêu quan sát, phân tích lưu lượng sẵn có.

## 2.3. Khái niệm về packet capture và PCAP

Packet capture là quá trình thu thập các gói tin đi qua một giao diện mạng. Khi một thiết bị giao tiếp trong mạng, dữ liệu được chia thành các gói tin nhỏ. Mỗi gói tin chứa các phần thông tin như địa chỉ nguồn, địa chỉ đích, giao thức sử dụng và dữ liệu tải theo.

PCAP là định dạng phổ biến dùng để lưu trữ dữ liệu gói tin đã bắt được. File PCAP thường được tạo bởi các công cụ như Wireshark, tcpdump hoặc các thư viện bắt gói tin. Việc sử dụng file PCAP giúp quá trình phân tích có thể được thực hiện ngoại tuyến, dễ kiểm thử và dễ tái hiện lại các tình huống mạng.

Trong hệ thống phát hiện tài sản mạng thụ động, dữ liệu đầu vào có thể đến từ hai nguồn chính:

- File PCAP đã được thu thập trước đó.
- Luồng gói tin trực tiếp từ một giao diện mạng.

File PCAP phù hợp cho kiểm thử, phân tích mẫu và đánh giá hệ thống. Trong khi đó, bắt gói tin trực tiếp phù hợp cho môi trường giám sát thực tế.

**[PLACEHOLDER HÌNH 2.3: Chèn hình minh họa quá trình bắt gói tin từ network interface và lưu thành file PCAP, sau đó đưa vào hệ thống phân tích]**

**Hình 2.3. Quá trình thu thập và phân tích gói tin từ file PCAP**

## 2.4. Các lớp giao thức liên quan

Để phân tích gói tin mạng, cần hiểu một số lớp giao thức cơ bản trong mô hình mạng. Trong phạm vi đề tài, các thông tin nhận diện thiết bị thường xuất hiện ở tầng liên kết dữ liệu, tầng mạng và tầng ứng dụng.

Ở tầng liên kết dữ liệu, địa chỉ MAC được sử dụng để định danh thiết bị trong mạng cục bộ. Các gói Ethernet chứa địa chỉ MAC nguồn và MAC đích. Đây là thông tin quan trọng để nhận diện thiết bị vật lý hoặc giao diện mạng.

Ở tầng mạng, giao thức IP cung cấp địa chỉ IP nguồn và IP đích. Địa chỉ IP giúp xác định thiết bị trong phạm vi mạng logic. Tuy nhiên, địa chỉ IP có thể thay đổi theo thời gian, đặc biệt trong các mạng sử dụng DHCP.

Ở tầng ứng dụng, nhiều giao thức có thể chứa thông tin bổ sung như hostname, domain name, tên dịch vụ hoặc loại thiết bị. Những thông tin này giúp hệ thống mô tả tài sản mạng rõ ràng hơn thay vì chỉ có IP và MAC.

**[PLACEHOLDER HÌNH 2.4: Chèn sơ đồ mô hình TCP/IP, đánh dấu các lớp có thông tin phục vụ phát hiện tài sản: Ethernet, IP, UDP/TCP và Application Protocols]**

**Hình 2.4. Các lớp giao thức liên quan đến quá trình phát hiện tài sản mạng**

## 2.5. Giao thức ARP

ARP, viết tắt của Address Resolution Protocol, là giao thức dùng để ánh xạ địa chỉ IP sang địa chỉ MAC trong mạng IPv4 cục bộ. Khi một thiết bị muốn gửi dữ liệu đến một địa chỉ IP trong cùng mạng LAN nhưng chưa biết địa chỉ MAC tương ứng, nó sẽ gửi gói ARP Request. Thiết bị sở hữu địa chỉ IP đó sẽ trả lời bằng gói ARP Reply chứa địa chỉ MAC của mình.

Đối với hệ thống phát hiện tài sản mạng, ARP là một nguồn thông tin quan trọng vì nó thường chứa cả địa chỉ IP và địa chỉ MAC. Khi phân tích gói ARP, hệ thống có thể trích xuất các thông tin như:

- Địa chỉ MAC nguồn.
- Địa chỉ IP nguồn.
- Địa chỉ MAC đích nếu có.
- Địa chỉ IP đích.
- Loại bản tin ARP Request hoặc ARP Reply.

Ví dụ, nếu hệ thống quan sát được một gói ARP Reply từ địa chỉ IP `192.168.1.10` với địa chỉ MAC `AA:BB:CC:DD:EE:FF`, hệ thống có thể ghi nhận rằng trong mạng có một thiết bị sử dụng IP và MAC tương ứng.

ARP có ưu điểm là phổ biến trong mạng IPv4 LAN, dễ phân tích và thường xuất hiện khi thiết bị kết nối hoặc giao tiếp với thiết bị khác. Tuy nhiên, ARP không cung cấp hostname hoặc thông tin loại thiết bị, nên cần kết hợp với các giao thức khác để có thông tin đầy đủ hơn.

## 2.6. Giao thức DHCP

DHCP, viết tắt của Dynamic Host Configuration Protocol, là giao thức dùng để cấp phát cấu hình mạng tự động cho thiết bị. Khi một thiết bị tham gia mạng, nó có thể gửi yêu cầu DHCP để nhận địa chỉ IP, subnet mask, gateway, DNS server và các thông tin cấu hình khác.

Quá trình DHCP thường gồm bốn bước chính:

- DHCP Discover: thiết bị tìm kiếm máy chủ DHCP.
- DHCP Offer: máy chủ DHCP đề xuất cấu hình mạng.
- DHCP Request: thiết bị yêu cầu sử dụng cấu hình được đề xuất.
- DHCP Acknowledgement: máy chủ xác nhận cấp phát cấu hình.

Trong phát hiện tài sản mạng thụ động, DHCP là nguồn dữ liệu rất có giá trị. Các gói DHCP có thể chứa địa chỉ MAC của thiết bị, địa chỉ IP được cấp, hostname của thiết bị và một số tùy chọn khác liên quan đến hệ điều hành hoặc loại thiết bị.

**[PLACEHOLDER HÌNH 2.5: Chèn sơ đồ luồng DHCP Discover, Offer, Request, Acknowledgement giữa Client và DHCP Server]**

**Hình 2.5. Quy trình cấp phát địa chỉ IP bằng giao thức DHCP**

Thông tin từ DHCP giúp hệ thống xác định thiết bị mới tham gia mạng và cập nhật thông tin IP, MAC, hostname một cách tương đối chính xác. Tuy nhiên, nếu thiết bị sử dụng IP tĩnh hoặc không phát sinh lưu lượng DHCP trong thời gian quan sát, hệ thống sẽ không thu thập được thông tin từ nguồn này.

## 2.7. Giao thức DNS

DNS, viết tắt của Domain Name System, là hệ thống phân giải tên miền thành địa chỉ IP. Trong mạng, thiết bị thường gửi truy vấn DNS khi cần truy cập một dịch vụ theo tên miền. Gói DNS có thể chứa tên miền được truy vấn, địa chỉ IP phản hồi và thông tin máy chủ DNS.

Đối với phát hiện tài sản mạng, DNS có thể hỗ trợ theo hai hướng. Thứ nhất, địa chỉ IP nguồn trong truy vấn DNS cho biết thiết bị nào đang thực hiện truy vấn. Thứ hai, một số bản ghi DNS nội bộ có thể phản ánh hostname hoặc dịch vụ trong mạng.

Tuy nhiên, DNS không phải lúc nào cũng cung cấp trực tiếp địa chỉ MAC. Vì vậy, để liên kết hostname hoặc hành vi truy vấn DNS với tài sản cụ thể, hệ thống cần kết hợp dữ liệu DNS với các nguồn khác như Ethernet, ARP hoặc DHCP.

## 2.8. Giao thức mDNS, NBNS và LLMNR

Ngoài DNS truyền thống, trong mạng LAN còn tồn tại nhiều giao thức phân giải tên cục bộ. Các giao thức này thường được thiết bị sử dụng để tìm kiếm dịch vụ hoặc phân giải tên khi không có máy chủ DNS nội bộ.

mDNS, viết tắt của Multicast DNS, cho phép thiết bị phân giải tên trong mạng cục bộ thông qua multicast. Giao thức này thường xuất hiện trong các môi trường có thiết bị Apple, Linux, máy in, thiết bị IoT hoặc các dịch vụ hỗ trợ Zeroconf/Bonjour. mDNS có thể chứa hostname hoặc tên dịch vụ, ví dụ như máy in, thiết bị chia sẻ file hoặc dịch vụ media.

NBNS, viết tắt của NetBIOS Name Service, thường xuất hiện trong môi trường Windows hoặc các hệ thống hỗ trợ NetBIOS. Giao thức này có thể chứa tên máy tính trong mạng nội bộ, từ đó giúp nhận diện thiết bị theo tên thay vì chỉ dựa vào địa chỉ IP.

LLMNR, viết tắt của Link-Local Multicast Name Resolution, là giao thức phân giải tên cục bộ được sử dụng khi DNS không khả dụng. LLMNR cũng có thể chứa hostname hoặc tên được truy vấn trong mạng LAN.

Các giao thức này rất hữu ích trong phát hiện tài sản mạng thụ động vì chúng thường chứa thông tin tên thiết bị. Tuy nhiên, dữ liệu có thể không đồng nhất, tên thiết bị có thể thay đổi và không phải thiết bị nào cũng sử dụng các giao thức này.

## 2.9. Một số giao thức hỗ trợ phát hiện dịch vụ

Bên cạnh các giao thức chính, một số giao thức khác cũng có thể cung cấp thông tin về thiết bị hoặc dịch vụ đang hoạt động trong mạng. Ví dụ, SSDP được sử dụng trong UPnP để thiết bị tự quảng bá hoặc tìm kiếm dịch vụ. Giao thức này có thể xuất hiện trên TV thông minh, camera, router hoặc thiết bị IoT.

Ngoài ra, các gói broadcast hoặc multicast khác trong mạng cũng có thể tiết lộ thông tin về loại dịch vụ, tên thiết bị hoặc phần mềm đang chạy. Việc phân tích các giao thức này giúp hệ thống mở rộng khả năng nhận diện, đặc biệt trong môi trường có nhiều thiết bị thông minh.

Bảng dưới đây tóm tắt một số giao thức thường dùng trong phát hiện tài sản mạng thụ động:

| Giao thức | Thông tin có thể thu thập | Vai trò trong hệ thống |
|---|---|---|
| Ethernet | MAC nguồn, MAC đích | Nhận diện giao diện mạng |
| ARP | IP, MAC | Ánh xạ IP với MAC |
| DHCP | IP, MAC, hostname | Phát hiện thiết bị mới tham gia mạng |
| DNS | IP, tên miền | Bổ sung thông tin truy vấn |
| mDNS | hostname, tên dịch vụ | Nhận diện thiết bị/dịch vụ cục bộ |
| NBNS | tên máy Windows | Bổ sung hostname |
| LLMNR | hostname cục bộ | Phân giải tên trong LAN |
| SSDP | tên dịch vụ, loại thiết bị | Phát hiện thiết bị IoT/UPnP |

## 2.10. Mô hình thông tin tài sản mạng

Sau khi trích xuất dữ liệu từ các gói tin, hệ thống cần tổng hợp thành một mô hình tài sản mạng thống nhất. Một tài sản mạng có thể được biểu diễn bằng nhiều thuộc tính khác nhau, trong đó các thuộc tính cơ bản gồm:

- Địa chỉ MAC.
- Địa chỉ IP.
- Hostname.
- Danh sách giao thức đã quan sát được.
- Nguồn phát hiện.
- Thời điểm phát hiện đầu tiên.
- Thời điểm cập nhật gần nhất.

Trong thực tế, thông tin của một thiết bị có thể xuất hiện rời rạc ở nhiều gói tin. Ví dụ, gói ARP cung cấp IP và MAC, trong khi gói DHCP cung cấp MAC và hostname. Nếu hệ thống biết hai thông tin này cùng thuộc về một địa chỉ MAC, hệ thống có thể hợp nhất chúng thành một tài sản duy nhất.

**[PLACEHOLDER HÌNH 2.6: Chèn sơ đồ minh họa việc gom thông tin từ ARP, DHCP, DNS, mDNS thành một bản ghi Asset thống nhất]**

**Hình 2.6. Tổng hợp thông tin từ nhiều giao thức thành một tài sản mạng**

Việc gom nhóm và cập nhật thông tin giúp giảm trùng lặp, tăng độ đầy đủ của dữ liệu và hỗ trợ người dùng quan sát tài sản mạng rõ ràng hơn.

## 2.11. Thách thức của phát hiện thụ động

Mặc dù phương pháp phát hiện thụ động có nhiều ưu điểm, nhưng cũng tồn tại một số thách thức.

Thứ nhất, hệ thống chỉ có thể phát hiện thiết bị có phát sinh lưu lượng mạng. Nếu một thiết bị không giao tiếp trong thời gian quan sát, hệ thống sẽ không thu thập được thông tin về thiết bị đó.

Thứ hai, thông tin thu thập được có thể không đầy đủ. Một số gói tin chỉ chứa địa chỉ IP, một số gói tin chỉ chứa hostname, trong khi địa chỉ MAC có thể không xuất hiện ở mọi vị trí quan sát, đặc biệt khi phân tích lưu lượng ngoài mạng LAN.

Thứ ba, dữ liệu có thể thay đổi theo thời gian. Một thiết bị có thể được cấp địa chỉ IP mới qua DHCP, hoặc một địa chỉ IP có thể được tái sử dụng cho thiết bị khác. Vì vậy, hệ thống cần lưu thời điểm phát hiện và cập nhật để hỗ trợ đánh giá tính chính xác của thông tin.

Thứ tư, mạng hiện đại có thể sử dụng mã hóa hoặc các cơ chế bảo vệ quyền riêng tư. Điều này làm giảm lượng thông tin có thể quan sát được ở tầng ứng dụng. Tuy nhiên, các thông tin cơ bản ở tầng liên kết và tầng mạng vẫn có thể hỗ trợ phát hiện tài sản ở mức nhất định.

## 2.12. Vấn đề bảo mật và quyền riêng tư

Phân tích lưu lượng mạng có thể thu thập nhiều thông tin nhạy cảm, bao gồm địa chỉ thiết bị, tên máy, tên dịch vụ và hành vi truy cập mạng. Vì vậy, hệ thống phát hiện tài sản mạng cần được triển khai trong phạm vi được phép và tuân thủ quy định của tổ chức.

Khi sử dụng file PCAP, cần lưu ý rằng file này có thể chứa dữ liệu nhạy cảm. Việc lưu trữ, chia sẻ hoặc sử dụng PCAP cho kiểm thử cần được kiểm soát. Trong môi trường thực tế, chỉ nên thu thập các trường thông tin cần thiết cho mục tiêu phát hiện tài sản, tránh lưu trữ nội dung payload không cần thiết.

Ngoài ra, kết quả phát hiện tài sản mạng cũng cần được bảo vệ vì danh sách thiết bị trong mạng có thể là thông tin quan trọng đối với an toàn hệ thống. Nếu bị lộ, dữ liệu này có thể hỗ trợ kẻ tấn công hiểu rõ cấu trúc mạng và lựa chọn mục tiêu.

## 2.13. Tổng kết chương

Chương này đã trình bày các cơ sở lý thuyết liên quan đến hệ thống phát hiện tài sản mạng thụ động. Nội dung bao gồm khái niệm tài sản mạng, sự khác nhau giữa phát hiện chủ động và thụ động, khái niệm packet capture, định dạng PCAP và các giao thức thường được sử dụng để trích xuất thông tin thiết bị như ARP, DHCP, DNS, mDNS, NBNS, LLMNR và SSDP.

Từ các nội dung đã trình bày, có thể thấy phát hiện tài sản mạng thụ động là phương pháp ít xâm lấn, phù hợp cho việc quan sát và kiểm kê thiết bị trong mạng nội bộ. Tuy nhiên, phương pháp này cũng phụ thuộc vào lượng lưu lượng quan sát được và cần cơ chế tổng hợp dữ liệu hợp lý để tạo ra danh sách tài sản chính xác hơn.

Các kiến thức trong chương này là nền tảng để xây dựng phần phân tích yêu cầu và thiết kế hệ thống ở các chương tiếp theo.
