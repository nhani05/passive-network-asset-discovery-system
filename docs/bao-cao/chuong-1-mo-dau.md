# CHƯƠNG 1. MỞ ĐẦU

## 1.1. Bối cảnh đề tài

Trong những năm gần đây, hệ thống mạng máy tính ngày càng đóng vai trò quan trọng trong hoạt động của cá nhân, tổ chức và doanh nghiệp. Hầu hết các hoạt động như trao đổi dữ liệu, truy cập dịch vụ nội bộ, quản lý thiết bị, vận hành hệ thống giám sát, làm việc từ xa và kết nối Internet đều phụ thuộc vào hạ tầng mạng. Cùng với sự phát triển đó, số lượng thiết bị kết nối vào mạng cũng tăng nhanh, không chỉ bao gồm máy tính cá nhân và máy chủ mà còn có điện thoại thông minh, máy in, camera IP, thiết bị IoT, thiết bị mạng và các hệ thống điều khiển chuyên dụng.

Sự đa dạng của các thiết bị giúp hệ thống mạng đáp ứng nhiều nhu cầu sử dụng khác nhau, nhưng đồng thời cũng làm cho việc quản lý tài sản mạng trở nên phức tạp hơn. Trong một mạng nội bộ, quản trị viên cần biết chính xác những thiết bị nào đang tồn tại, thiết bị nào được phép sử dụng, thiết bị nào mới xuất hiện, thiết bị nào có dấu hiệu bất thường và thiết bị nào không còn hoạt động. Nếu không có cơ chế kiểm kê và giám sát phù hợp, hệ thống mạng rất dễ xuất hiện các thiết bị không xác định, gây khó khăn cho công tác vận hành và bảo mật.

**[PLACEHOLDER HÌNH 1.1: Chèn sơ đồ minh họa một mạng LAN có nhiều loại thiết bị như máy tính, máy chủ, điện thoại, camera IP, máy in, router/switch và thiết bị IoT]**

**Hình 1.1. Mô hình minh họa sự đa dạng thiết bị trong một hệ thống mạng nội bộ**

Trong lĩnh vực an toàn thông tin, việc nhận diện và quản lý tài sản mạng là một bước nền tảng. Một tổ chức không thể bảo vệ tốt hệ thống nếu không biết rõ trong mạng của mình có những thiết bị nào. Các thiết bị không được quản lý có thể trở thành điểm yếu bảo mật, đặc biệt khi chúng sử dụng cấu hình mặc định, phần mềm lỗi thời hoặc không được cập nhật bản vá. Vì vậy, phát hiện tài sản mạng là một yêu cầu cần thiết trong quá trình giám sát và bảo vệ hạ tầng mạng.

## 1.2. Lý do chọn đề tài

Hiện nay, có nhiều phương pháp được sử dụng để phát hiện thiết bị trong mạng. Một trong những phương pháp phổ biến là phát hiện chủ động, trong đó hệ thống gửi các gói tin thăm dò đến các địa chỉ IP trong mạng để xác định thiết bị đang hoạt động. Ví dụ, công cụ quét mạng có thể gửi gói ICMP, TCP, UDP hoặc ARP để kiểm tra phản hồi từ các thiết bị.

Tuy nhiên, phương pháp quét chủ động có một số hạn chế. Thứ nhất, việc gửi nhiều gói tin thăm dò có thể làm tăng lưu lượng mạng, đặc biệt trong các hệ thống có quy mô lớn. Thứ hai, một số thiết bị nhạy cảm có thể phản ứng không ổn định khi nhận lưu lượng quét. Thứ ba, tường lửa hoặc hệ thống phát hiện xâm nhập có thể chặn hoặc cảnh báo các hành vi quét mạng. Thứ tư, trong một số môi trường yêu cầu tính ổn định cao, quản trị viên không muốn công cụ giám sát can thiệp trực tiếp vào thiết bị đang vận hành.

Từ những hạn chế trên, phương pháp phát hiện thụ động trở thành một hướng tiếp cận phù hợp. Thay vì gửi gói tin đến thiết bị, hệ thống chỉ quan sát lưu lượng mạng sẵn có và trích xuất thông tin từ các gói tin được thu thập. Nhiều giao thức trong mạng LAN như ARP, DHCP, DNS, mDNS, NBNS và LLMNR thường chứa các thông tin có giá trị như địa chỉ IP, địa chỉ MAC, hostname hoặc tên dịch vụ. Thông qua việc phân tích các gói tin này, hệ thống có thể suy luận và xây dựng danh sách tài sản mạng.

**[PLACEHOLDER HÌNH 1.2: Chèn hình so sánh hai phương pháp Active Discovery và Passive Discovery. Bên trái là hệ thống gửi gói tin quét đến thiết bị; bên phải là hệ thống chỉ lắng nghe và phân tích lưu lượng mạng]**

**Hình 1.2. So sánh phương pháp phát hiện chủ động và phát hiện thụ động**

Vì vậy, đề tài **“Passive Network Asset Discovery System”** được lựa chọn nhằm nghiên cứu và xây dựng một hệ thống hỗ trợ phát hiện tài sản mạng bằng phương pháp thụ động. Hệ thống hướng đến việc thu thập lưu lượng mạng, phân tích các gói tin, trích xuất thông tin nhận diện và tổng hợp thành danh sách thiết bị. Đây là một hướng tiếp cận có tính thực tiễn, phù hợp với nhu cầu giám sát mạng ít xâm lấn và hỗ trợ công tác quản trị tài sản mạng.

## 1.3. Vấn đề đặt ra

Trong quá trình quản trị mạng, một trong những khó khăn lớn là duy trì danh sách tài sản mạng chính xác và cập nhật. Trên thực tế, thiết bị có thể được thêm vào hoặc rời khỏi mạng bất kỳ lúc nào. Người dùng có thể kết nối thiết bị cá nhân, thiết bị IoT có thể tự động tham gia mạng, hoặc một thiết bị cũ có thể được thay đổi địa chỉ IP. Nếu chỉ kiểm kê thủ công, thông tin tài sản rất dễ bị thiếu sót hoặc lỗi thời.

Bên cạnh đó, một thiết bị mạng thường không cung cấp đầy đủ thông tin trong một gói tin duy nhất. Ví dụ, địa chỉ MAC có thể xuất hiện trong gói ARP, địa chỉ IP có thể xuất hiện trong nhiều loại gói tin, còn hostname có thể được tìm thấy trong DHCP, DNS, mDNS hoặc NBNS. Do đó, hệ thống cần có khả năng thu thập thông tin từ nhiều nguồn khác nhau, sau đó tổng hợp các dữ liệu rời rạc thành một bản ghi tài sản thống nhất.

Một vấn đề khác là dữ liệu mạng thường có khối lượng lớn và liên tục thay đổi. Hệ thống cần xử lý gói tin một cách ổn định, tránh tạo ra nhiều bản ghi trùng lặp cho cùng một thiết bị. Đồng thời, hệ thống cũng cần ghi nhận thời điểm phát hiện đầu tiên, thời điểm cập nhật gần nhất và nguồn dữ liệu giúp nhận diện thiết bị.

Từ các vấn đề trên, đề tài cần giải quyết các câu hỏi chính sau:

- Làm thế nào để thu thập lưu lượng mạng phục vụ phân tích tài sản?
- Những giao thức nào có thể cung cấp thông tin hữu ích để nhận diện thiết bị?
- Làm thế nào để trích xuất các thông tin như IP, MAC, hostname và giao thức xuất hiện?
- Làm thế nào để gom nhóm nhiều thông tin rời rạc thành một tài sản mạng?
- Làm thế nào để trình bày kết quả phát hiện một cách rõ ràng, dễ kiểm tra?

## 1.4. Mục tiêu của đề tài

Mục tiêu tổng quát của đề tài là xây dựng một hệ thống phát hiện tài sản mạng thụ động, có khả năng phân tích lưu lượng mạng để nhận diện các thiết bị xuất hiện trong mạng nội bộ.

Các mục tiêu cụ thể bao gồm:

- Nghiên cứu nguyên lý hoạt động của phương pháp phát hiện tài sản mạng thụ động.
- Tìm hiểu các giao thức mạng thường chứa thông tin nhận diện thiết bị như ARP, DHCP, DNS, mDNS, NBNS và LLMNR.
- Xây dựng chức năng đọc dữ liệu từ file PCAP để phục vụ kiểm thử và phân tích ngoại tuyến.
- Xây dựng khả năng thu thập gói tin trực tiếp từ giao diện mạng trong môi trường phù hợp.
- Phân tích gói tin để trích xuất thông tin địa chỉ IP, địa chỉ MAC, hostname, giao thức và thời điểm phát hiện.
- Tổng hợp thông tin thu thập được thành danh sách tài sản mạng, hạn chế trùng lặp dữ liệu.
- Cung cấp kết quả đầu ra dưới dạng dễ quan sát, phục vụ quá trình kiểm kê và đánh giá hệ thống mạng.

## 1.5. Đối tượng và phạm vi nghiên cứu

Đối tượng nghiên cứu của đề tài là các thiết bị xuất hiện trong mạng nội bộ và các gói tin mạng có khả năng cung cấp thông tin nhận diện thiết bị. Các thiết bị này có thể là máy tính cá nhân, máy chủ, điện thoại, máy in, camera IP, router, switch hoặc thiết bị IoT.

Phạm vi nghiên cứu tập trung vào phương pháp phát hiện thụ động trong mạng LAN. Hệ thống không thực hiện quét chủ động, không gửi gói tin thăm dò và không can thiệp vào quá trình giao tiếp của các thiết bị. Dữ liệu đầu vào của hệ thống là lưu lượng mạng đã được thu thập từ file PCAP hoặc từ giao diện mạng.

Về mặt chức năng, đề tài tập trung vào việc phát hiện thông tin cơ bản của tài sản mạng như địa chỉ IP, địa chỉ MAC, hostname, giao thức phát hiện và thời điểm xuất hiện. Các chức năng nâng cao như đánh giá lỗ hổng bảo mật, phát hiện tấn công, phân loại thiết bị bằng học máy, tích hợp SIEM hoặc cảnh báo thời gian thực quy mô lớn chưa nằm trong phạm vi chính của đề tài.

Một hạn chế cần lưu ý là phương pháp thụ động chỉ phát hiện được các thiết bị có phát sinh lưu lượng trong khoảng thời gian quan sát. Nếu một thiết bị đang tắt, không giao tiếp hoặc không xuất hiện trong dữ liệu PCAP, hệ thống có thể không nhận diện được thiết bị đó. Đây là đặc điểm tự nhiên của phương pháp phát hiện thụ động.

## 1.6. Phương pháp thực hiện

Đề tài được thực hiện theo hướng kết hợp giữa nghiên cứu lý thuyết và xây dựng hệ thống thử nghiệm. Trước hết, các kiến thức nền tảng về mạng máy tính, phân tích gói tin và các giao thức mạng phổ biến được nghiên cứu để xác định những thông tin có thể khai thác từ lưu lượng mạng.

Sau đó, hệ thống được thiết kế thành các thành phần chính gồm: thu thập dữ liệu, phân tích gói tin, trích xuất thông tin, tổng hợp tài sản và xuất kết quả. Mỗi thành phần đảm nhiệm một nhiệm vụ riêng nhằm giúp hệ thống dễ kiểm thử, dễ bảo trì và có khả năng mở rộng trong tương lai.

**[PLACEHOLDER HÌNH 1.3: Chèn sơ đồ luồng xử lý tổng quát gồm các bước: PCAP/Live Capture → Packet Parser → Protocol Analyzer → Asset Aggregator → Output Report]**

**Hình 1.3. Luồng xử lý tổng quát của hệ thống Passive Network Asset Discovery**

Trong quá trình triển khai, hệ thống đọc từng gói tin từ nguồn dữ liệu, xác định loại giao thức, trích xuất các trường thông tin cần thiết và cập nhật vào danh sách tài sản. Nếu một thiết bị đã tồn tại trong danh sách, hệ thống sẽ bổ sung hoặc cập nhật thông tin thay vì tạo bản ghi mới. Cách làm này giúp giảm dữ liệu trùng lặp và tạo ra danh sách tài sản có tính tổng hợp hơn.

Cuối cùng, hệ thống được kiểm thử bằng các mẫu dữ liệu mạng khác nhau để đánh giá khả năng phát hiện thiết bị, mức độ đầy đủ của thông tin và tính ổn định khi xử lý dữ liệu. Kết quả kiểm thử được sử dụng để nhận xét ưu điểm, hạn chế và đề xuất hướng phát triển.

## 1.7. Ý nghĩa của đề tài

Về mặt thực tiễn, đề tài hỗ trợ quản trị viên mạng trong việc kiểm kê và giám sát tài sản mạng. Thay vì phải ghi nhận thiết bị thủ công, hệ thống có thể tự động phân tích lưu lượng mạng để tạo danh sách thiết bị đang hoạt động. Điều này giúp tiết kiệm thời gian, giảm sai sót và hỗ trợ phát hiện các thiết bị lạ trong mạng.

Về mặt an toàn thông tin, việc nhận diện tài sản mạng là một bước quan trọng trong quá trình bảo vệ hệ thống. Khi có danh sách tài sản tương đối đầy đủ, quản trị viên có thể dễ dàng phát hiện thiết bị bất thường, kiểm tra thiết bị chưa được quản lý và đánh giá phạm vi cần bảo vệ.

Về mặt học thuật, đề tài giúp người thực hiện nắm vững hơn các kiến thức về phân tích gói tin, giao thức mạng, xử lý dữ liệu và thiết kế hệ thống giám sát. Đây là nền tảng để phát triển các hệ thống phức tạp hơn như giám sát an ninh mạng, phát hiện bất thường hoặc quản lý tài sản tự động.

## 1.8. Bố cục báo cáo

Báo cáo được tổ chức thành các chương như sau:

**Chương 1: Mở đầu** trình bày bối cảnh, lý do chọn đề tài, vấn đề đặt ra, mục tiêu, phạm vi nghiên cứu, phương pháp thực hiện, ý nghĩa và bố cục báo cáo.

**Chương 2: Cơ sở lý thuyết** trình bày các kiến thức nền tảng về phát hiện tài sản mạng thụ động, phân tích gói tin và các giao thức mạng liên quan.

**Chương 3: Phân tích yêu cầu** mô tả yêu cầu chức năng, yêu cầu phi chức năng và các ràng buộc kỹ thuật của hệ thống.

**Chương 4: Thiết kế hệ thống** trình bày kiến trúc tổng thể, các thành phần chính, luồng xử lý dữ liệu và mô hình thông tin tài sản mạng.

**Chương 5: Triển khai hệ thống** mô tả công nghệ sử dụng, cấu trúc mã nguồn, cách xử lý gói tin và cách vận hành hệ thống.

**Chương 6: Kiểm thử và đánh giá** trình bày môi trường kiểm thử, dữ liệu kiểm thử, các trường hợp kiểm thử, kết quả đạt được và đánh giá hệ thống.

**Chương 7: Kết luận và hướng phát triển** tổng kết kết quả thực hiện đề tài, nêu hạn chế còn tồn tại và đề xuất các hướng cải tiến trong tương lai.
