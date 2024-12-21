##DataComm-TCPChat

Bu proje, Windows sistemleri için Winsock kütüphanesi kullanılarak C'de uygulanan basit bir istemci-sunucu sohbet uygulaması gibi görünmektedir. Sunucu, birden fazla istemcinin bağlanmasına, genel ve özel mesaj alışverişinde bulunmasına, çevrimiçi kullanıcıları listelemesine ve özel komutları işlemesine olanak tanır. Ek olarak, proje hata işleme, eşzamanlı istemci bağlantıları için çoklu iş parçacığı ve sağlama toplamları ve CRC aracılığıyla mesaj bütünlüğü doğrulaması içerir.

İşte temel bileşenlerin kısa bir açıklaması:

Sunucu (server.c): Sunucu Başlatma:

Sunucu Winsock'u başlatır, bir soket oluşturur ve bunu belirli bir porta bağlar. Gelen bağlantıları dinler ve istemcileri kabul eder. Kullanıcı İşleme:

Her bağlı istemci, IP adresi, soket ve kullanıcı adı gibi bilgileri saklayan bir struct User ile temsil edilir. Aynı anda en fazla MAXUSER istemci bağlanabilir. İstemci Konuları:

Her istemci bağlantısı özel bir iş parçacığı (ServiceClient) tarafından yönetilir. İş parçacığı genel ve özel mesajları, kullanıcı çıkışlarını ve çevrimiçi kullanıcıları listeler. Mesaj Bozulması İşleme:

Bir istemci bozuk bir mesaj tespit ederse, sunucudan düzeltilmiş mesajı yeniden göndermesini ister (MERR komutu). Günlükleme:

Özel mesajlar, konuşmaya dahil olan tarih, saat ve kullanıcı adlarına göre dosya adlarına sahip dosyalara kaydedilir. İstemci (client.c): İstemci Başlatma:

İstemci Winsock'u başlatır, bir soket oluşturur ve belirtilen ana bilgisayar adını ve bağlantı noktasını kullanarak sunucuya bağlanır. Kullanıcı Kaydı:

İstemci bağlantı sırasında kullanıcı adını sunucuya kaydeder. Mesaj İşleme:

İstemci, sunucudan sürekli olarak mesaj almak ve işlemek için özel bir iş parçacığına (ReceiveChat) sahiptir. Mesajlar genel, özel ve düzeltilmiş mesajları içerebilir. Mesaj Bütünlüğü Doğrulaması:

İstemci, alınan mesajların bütünlüğünü sağlama toplamları ve CRC8 kullanarak doğrular. Bir mesajın bozuk olduğu tespit edilirse, istemci sunucudan düzeltilmiş bir mesaj talep eder. Kullanıcı Girdisi:

İstemci sürekli olarak konsoldan kullanıcı girdilerini okur ve sunucuya mesajlar gönderir. Genel Açıklama: Proje, birden fazla istemcinin bir sunucuya bağlanmasına, genel ve özel mesaj alışverişinde bulunmasına ve temel sohbet işlevlerini gerçekleştirmesine olanak tanır. Mesaj bütünlüğü sağlama toplamları ve CRC ile sağlanır ve kullanıcıları listeleme ve sohbetten çıkma gibi özel komutlar desteklenir. Sunucu, iş parçacıkları kullanarak aynı anda birden fazla istemci bağlantısını işler.
