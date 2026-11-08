#pragma once
#include "lang.h"

// Texts of the mod's settings rows, one per language in the order of Lang:
// EN, FR, IT, DE, ES, PT_BR, PL, RU, JA, KO, ZH_CN, ZH_TW, TH.
namespace str
{
inline const Text Off = {L"Off", L"Désactivé", L"Disattivato", L"Aus", L"Desactivado", L"Desativado", L"Wył.", L"Выкл.", L"オフ", L"끄기", L"关", L"關", L"ปิด"};
inline const Text On = {L"On", L"Activé", L"Attivato", L"An", L"Activado", L"Ativado", L"Wł.", L"Вкл.", L"オン", L"켜기", L"开", L"開", L"เปิด"};
inline const Text Default = {L"Default", L"Par défaut", L"Predefinito", L"Standard", L"Predeterminado", L"Padrão", L"Domyślne", L"По умолчанию", L"デフォルト", L"기본값", L"默认", L"預設", L"ค่าเริ่มต้น"};

inline const Text Autoloot = {L"Autoloot", L"Ramassage automatique", L"Raccolta automatica", L"Automatisches Aufsammeln", L"Recogida automática", L"Coleta automática",
                              L"Automatyczne podnoszenie", L"Автоподбор", L"自動拾得", L"자동 줍기", L"自动拾取", L"自動拾取", L"เก็บของอัตโนมัติ"};
inline const Text AutolootHelp = {L"Picks up items automatically.",
                                  L"Ramasse les objets automatiquement.",
                                  L"Raccoglie automaticamente gli oggetti.",
                                  L"Hebt Gegenstände automatisch auf.",
                                  L"Recoge los objetos automáticamente.",
                                  L"Coleta itens automaticamente.",
                                  L"Automatycznie podnosi przedmioty.",
                                  L"Автоматически подбирает предметы.",
                                  L"アイテムを自動で拾います。",
                                  L"아이템을 자동으로 줍습니다.",
                                  L"自动拾取物品。",
                                  L"自動拾取物品。",
                                  L"เก็บไอเทมโดยอัตโนมัติ"};

inline const Text SkipLogos = {L"Skip Logos (restart)", L"Passer les logos (redémarrage)", L"Salta i loghi (riavvio)", L"Logos überspringen (Neustart)",
                               L"Omitir logotipos (reinicio)", L"Pular logotipos (reiniciar)", L"Pomiń logo (restart)", L"Пропуск логотипов (перезапуск)",
                               L"ロゴをスキップ（再起動）", L"로고 건너뛰기 (재시작)", L"跳过开场标志（需重启）", L"跳過開場標誌（需重新啟動）", L"ข้ามโลโก้ (รีสตาร์ท)"};
inline const Text SkipLogosHelp = {L"Skips the start-up logos. Takes effect after restarting the game.",
                                   L"Passe les logos au démarrage. Prend effet après le redémarrage du jeu.",
                                   L"Salta i loghi all'avvio. Ha effetto dopo il riavvio del gioco.",
                                   L"Überspringt die Logos beim Start. Wirkt nach einem Neustart des Spiels.",
                                   L"Omite los logotipos del inicio. Se aplica tras reiniciar el juego.",
                                   L"Pula os logotipos de inicialização. Entra em vigor após reiniciar o jogo.",
                                   L"Pomija logo przy uruchamianiu. Działa po ponownym uruchomieniu gry.",
                                   L"Пропускает логотипы при запуске. Вступает в силу после перезапуска игры.",
                                   L"起動時のロゴをスキップします。ゲームの再起動後に反映されます。",
                                   L"시작 로고를 건너뜁니다. 게임을 다시 시작하면 적용됩니다.",
                                   L"跳过启动时的标志画面。重启游戏后生效。",
                                   L"跳過啟動時的標誌畫面。重新啟動遊戲後生效。",
                                   L"ข้ามโลโก้ตอนเริ่มเกม มีผลหลังจากรีสตาร์ทเกม"};

inline const Text DeathPenalties = {L"Disable Death Penalties", L"Désactiver les pénalités de mort", L"Disattiva penalità di morte", L"Todesstrafen deaktivieren",
                                    L"Desactivar penalizaciones por muerte", L"Desativar penalidades de morte", L"Wyłącz kary za śmierć", L"Отключить штрафы за смерть",
                                    L"死亡ペナルティを無効化", L"사망 페널티 끄기", L"禁用死亡惩罚", L"停用死亡懲罰", L"ปิดบทลงโทษเมื่อตาย"};
inline const Text DeathPenaltiesHelp = {L"No Sen or experience is lost on death. Dragonrot is not affected.",
                                        L"Aucun sen ni point d'expérience n'est perdu à la mort. La peste du dragon n'est pas affectée.",
                                        L"Alla morte non si perdono sen né esperienza. Il morbo del drago non è influenzato.",
                                        L"Beim Tod gehen weder Sen noch Erfahrung verloren. Die Drachenfäule ist nicht betroffen.",
                                        L"No se pierden sen ni experiencia al morir. La podredumbre de dragón no se ve afectada.",
                                        L"Nenhum sen ou experiência é perdido ao morrer. A podridão do dragão não é afetada.",
                                        L"Po śmierci nie traci się senów ani doświadczenia. Nie wpływa na smoczą zgniliznę.",
                                        L"При смерти не теряются сэны и опыт. На драконью гниль не влияет.",
                                        L"死亡時に銭と経験を失いません。竜咳には影響しません。",
                                        L"사망 시 센과 경험치를 잃지 않습니다. 용해에는 영향을 주지 않습니다.",
                                        L"死亡时不会损失钱和经验。不影响龙咳。",
                                        L"死亡時不會損失錢與經驗。不影響龍咳。",
                                        L"ไม่เสียเงินเซนหรือค่าประสบการณ์เมื่อตาย ไม่มีผลต่อโรคมังกร"};

inline const Text Dragonrot = {L"Disable Dragonrot", L"Désactiver la peste du dragon", L"Disattiva morbo del drago", L"Drachenfäule deaktivieren",
                               L"Desactivar podredumbre de dragón", L"Desativar podridão do dragão", L"Wyłącz smoczą zgniliznę", L"Отключить драконью гниль",
                               L"竜咳を無効化", L"용해 끄기", L"禁用龙咳", L"停用龍咳", L"ปิดโรคมังกร"};
inline const Text DragonrotHelp = {L"Dragonrot no longer spreads to NPCs.",
                                   L"La peste du dragon ne se propage plus aux PNJ.",
                                   L"Il morbo del drago non si diffonde più ai PNG.",
                                   L"Die Drachenfäule breitet sich nicht mehr auf NPCs aus.",
                                   L"La podredumbre de dragón ya no se propaga a los PNJ.",
                                   L"A podridão do dragão não se espalha mais para os NPCs.",
                                   L"Smocza zgnilizna nie rozprzestrzenia się już na NPC.",
                                   L"Драконья гниль больше не распространяется на NPC.",
                                   L"竜咳がNPCに広がらなくなります。",
                                   L"용해가 더 이상 NPC에게 퍼지지 않습니다.",
                                   L"龙咳不再传染给NPC。",
                                   L"龍咳不再傳染給NPC。",
                                   L"โรคมังกรจะไม่แพร่ไปยัง NPC อีก"};

inline const Text CamReset = {L"Disable Camera Reset", L"Désactiver la réinitialisation de la caméra", L"Disattiva reimpostazione telecamera",
                              L"Kamera-Zurücksetzen deaktivieren", L"Desactivar reinicio de cámara", L"Desativar redefinição da câmera", L"Wyłącz resetowanie kamery",
                              L"Отключить сброс камеры", L"カメラリセットを無効化", L"카메라 초기화 끄기", L"禁用镜头重置", L"停用鏡頭重置", L"ปิดการรีเซ็ตกล้อง"};
inline const Text CamResetHelp = {L"Locking on with no target in sight no longer re-centers the camera.",
                                  L"Verrouiller sans cible en vue ne recentre plus la caméra.",
                                  L"Agganciare senza bersagli in vista non ricentra più la telecamera.",
                                  L"Anvisieren ohne Ziel in Sicht zentriert die Kamera nicht mehr.",
                                  L"Fijar sin ningún objetivo a la vista ya no recentra la cámara.",
                                  L"Travar a mira sem alvo à vista não recentraliza mais a câmera.",
                                  L"Namierzanie bez celu w zasięgu wzroku nie centruje już kamery.",
                                  L"Захват цели, когда врагов не видно, больше не выравнивает камеру.",
                                  L"視界に敵がいない状態でロックオンしても、カメラが正面に戻らなくなります。",
                                  L"시야에 대상이 없을 때 록온해도 카메라가 다시 정렬되지 않습니다.",
                                  L"视野中没有目标时锁定不再重置镜头。",
                                  L"視野中沒有目標時鎖定不再重置鏡頭。",
                                  L"การล็อกเป้าเมื่อไม่มีเป้าหมายในสายตาจะไม่จัดกล้องกลับกลางอีก"};

inline const Text LockOnDot = {L"Hide Lock-on Dot", L"Masquer le point de verrouillage", L"Nascondi punto di aggancio",
                               L"Anvisier-Punkt ausblenden", L"Ocultar punto de fijación", L"Ocultar ponto de trava", L"Ukryj punkt namierzania",
                               L"Скрыть точку захвата цели", L"ロックオンの点を非表示", L"록온 점 숨기기", L"隐藏锁定圆点", L"隱藏鎖定圓點", L"ซ่อนจุดล็อกเป้า"};
inline const Text LockOnDotHelp = {L"Hides the dot that marks the locked-on target.",
                                   L"Masque le point qui marque la cible verrouillée.",
                                   L"Nasconde il punto che indica il bersaglio agganciato.",
                                   L"Blendet den Punkt aus, der das anvisierte Ziel markiert.",
                                   L"Oculta el punto que marca el objetivo fijado.",
                                   L"Oculta o ponto que marca o alvo travado.",
                                   L"Ukrywa punkt oznaczający namierzony cel.",
                                   L"Скрывает точку, отмечающую захваченную цель.",
                                   L"ロックオン中の敵を示す点を非表示にします。",
                                   L"록온한 대상을 표시하는 점을 숨깁니다.",
                                   L"隐藏标记锁定目标的圆点。",
                                   L"隱藏標記鎖定目標的圓點。",
                                   L"ซ่อนจุดที่ระบุเป้าหมายที่ล็อกอยู่"};

inline const Text CamAutorotate = {L"Disable Camera Autorotate", L"Désactiver la rotation auto de la caméra", L"Disattiva rotazione automatica telecamera",
                                   L"Automatische Kameradrehung deaktivieren", L"Desactivar giro automático de cámara", L"Desativar rotação automática da câmera",
                                   L"Wyłącz automatyczny obrót kamery", L"Отключить автоповорот камеры", L"カメラ自動回転を無効化", L"카메라 자동 회전 끄기",
                                   L"禁用镜头自动旋转", L"停用鏡頭自動旋轉", L"ปิดการหมุนกล้องอัตโนมัติ"};
inline const Text CamAutorotateHelp = {L"Disables automatic camera rotation while moving.",
                                       L"Désactive la rotation automatique de la caméra en mouvement.",
                                       L"Disattiva la rotazione automatica della telecamera durante il movimento.",
                                       L"Deaktiviert die automatische Kameradrehung beim Laufen.",
                                       L"Desactiva el giro automático de la cámara al moverse.",
                                       L"Desativa a rotação automática da câmera ao se mover.",
                                       L"Wyłącza automatyczny obrót kamery podczas ruchu.",
                                       L"Отключает автоматический поворот камеры при движении.",
                                       L"移動中のカメラの自動回転を無効にします。",
                                       L"이동 중 카메라 자동 회전을 끕니다.",
                                       L"禁用移动时的镜头自动旋转。",
                                       L"停用移動時的鏡頭自動旋轉。",
                                       L"ปิดการหมุนกล้องอัตโนมัติขณะเคลื่อนที่"};

inline const Text Fov = {L"Field of View", L"Champ de vision", L"Campo visivo", L"Sichtfeld", L"Campo de visión", L"Campo de visão", L"Pole widzenia",
                         L"Поле зрения", L"視野角", L"시야각", L"视野", L"視野", L"มุมมองภาพ"};
inline const Text FovHelp = {L"Adjusts the camera field of view.",
                             L"Règle le champ de vision de la caméra.",
                             L"Regola il campo visivo della telecamera.",
                             L"Passt das Sichtfeld der Kamera an.",
                             L"Ajusta el campo de visión de la cámara.",
                             L"Ajusta o campo de visão da câmera.",
                             L"Dostosowuje pole widzenia kamery.",
                             L"Настраивает поле зрения камеры.",
                             L"カメラの視野角を調整します。",
                             L"카메라 시야각을 조절합니다.",
                             L"调整镜头视野。",
                             L"調整鏡頭視野。",
                             L"ปรับมุมมองภาพของกล้อง"};

inline const Text FpsLimit = {L"FPS Limit", L"Limite de FPS", L"Limite FPS", L"FPS-Begrenzung", L"Límite de FPS", L"Limite de FPS", L"Limit FPS",
                              L"Ограничение FPS", L"FPS上限", L"FPS 제한", L"帧率上限", L"影格率上限", L"จำกัด FPS"};
inline const Text FpsLimitHelp = {L"Frame rate cap. 60 FPS is the game's default; higher values unlock the frame rate.",
                                  L"Limite d'images par seconde. 60 FPS est la valeur par défaut du jeu ; les valeurs supérieures débloquent la fréquence d'images.",
                                  L"Limite di fotogrammi. 60 FPS è il valore predefinito del gioco; valori più alti sbloccano il frame rate.",
                                  L"Bildratenbegrenzung. 60 FPS ist der Standard des Spiels; höhere Werte heben die Bildratensperre auf.",
                                  L"Límite de fotogramas. 60 FPS es el valor predeterminado del juego; los valores más altos desbloquean la tasa de fotogramas.",
                                  L"Limite de quadros. 60 FPS é o padrão do jogo; valores maiores desbloqueiam a taxa de quadros.",
                                  L"Limit klatek na sekundę. 60 FPS to domyślna wartość gry; wyższe wartości odblokowują liczbę klatek.",
                                  L"Ограничение частоты кадров. 60 FPS — значение игры по умолчанию; большие значения снимают ограничение.",
                                  L"フレームレートの上限です。60 FPSがゲームの既定値で、それより高い値でフレームレートの制限を解除します。",
                                  L"프레임 속도 제한입니다. 60 FPS가 게임 기본값이며, 더 높은 값은 프레임 속도 제한을 해제합니다.",
                                  L"帧率上限。60 FPS 为游戏默认值；更高的数值会解锁帧率。",
                                  L"影格率上限。60 FPS 為遊戲預設值；更高的數值會解除影格率限制。",
                                  L"จำกัดอัตราเฟรม 60 FPS คือค่าเริ่มต้นของเกม ค่าที่สูงกว่าจะปลดล็อกอัตราเฟรม"};
inline const Text Unlimited = {L"Unlimited", L"Illimité", L"Illimitato", L"Unbegrenzt", L"Ilimitado", L"Ilimitado", L"Bez limitu",
                               L"Без ограничения", L"無制限", L"무제한", L"无限制", L"無限制", L"ไม่จำกัด"};

inline const Text RefreshRate = {L"Refresh Rate", L"Fréquence de rafraîchissement", L"Frequenza di aggiornamento", L"Bildwiederholrate", L"Frecuencia de actualización",
                                 L"Taxa de atualização", L"Częstotliwość odświeżania", L"Частота обновления", L"リフレッシュレート", L"주사율", L"刷新率", L"更新率", L"อัตรารีเฟรช"};
inline const Text RefreshRateHelp = {L"Refresh rate of the display in fullscreen. Auto leaves it to the graphics driver.",
                                     L"Fréquence de l'écran en plein écran. Auto la laisse au pilote graphique.",
                                     L"Frequenza dello schermo a schermo intero. Auto la lascia al driver grafico.",
                                     L"Bildwiederholrate im Vollbild. Auto überlässt sie dem Grafiktreiber.",
                                     L"Frecuencia de la pantalla en pantalla completa. Automático la deja al controlador gráfico.",
                                     L"Taxa de atualização da tela em tela cheia. Automático deixa a escolha para o driver de vídeo.",
                                     L"Częstotliwość odświeżania w trybie pełnoekranowym. Automatycznie pozostawia wybór sterownikowi grafiki.",
                                     L"Частота обновления экрана в полноэкранном режиме. «Авто» оставляет выбор драйверу видеокарты.",
                                     L"フルスクリーン時のリフレッシュレートです。自動ではグラフィックドライバーに任せます。",
                                     L"전체 화면에서의 주사율입니다. 자동은 그래픽 드라이버에 맡깁니다.",
                                     L"全屏时的刷新率。自动由显卡驱动决定。",
                                     L"全螢幕時的更新率。自動由顯示卡驅動程式決定。",
                                     L"อัตรารีเฟรชของจอในโหมดเต็มจอ อัตโนมัติจะให้ไดรเวอร์กราฟิกเลือก"};
inline const Text Auto = {L"Auto", L"Auto", L"Auto", L"Auto", L"Automático", L"Automático", L"Automatycznie", L"Авто", L"自動", L"자동", L"自动", L"自動", L"อัตโนมัติ"};

inline const Text VSync = {L"VSync", L"Synchronisation verticale", L"Sincronizzazione verticale", L"VSync", L"Sincronización vertical", L"Sincronização vertical",
                           L"Synchronizacja pionowa", L"Вертикальная синхронизация", L"垂直同期", L"수직 동기화", L"垂直同步", L"垂直同步", L"VSync"};
inline const Text VSyncHelp = {L"Synchronizes presentation with the display.",
                               L"Synchronise l'affichage des images avec l'écran.",
                               L"Sincronizza la visualizzazione delle immagini con lo schermo.",
                               L"Synchronisiert die Bildausgabe mit dem Bildschirm.",
                               L"Sincroniza la presentación de imágenes con la pantalla.",
                               L"Sincroniza a exibição das imagens com a tela.",
                               L"Synchronizuje wyświetlanie klatek z ekranem.",
                               L"Синхронизирует вывод кадров с дисплеем.",
                               L"画面の表示をディスプレイと同期します。",
                               L"화면 출력을 디스플레이와 동기화합니다.",
                               L"使画面输出与显示器同步。",
                               L"使畫面輸出與顯示器同步。",
                               L"ซิงค์การแสดงภาพกับจอแสดงผล"};

inline const Text AspectRatio = {L"Aspect Ratio", L"Format d'image", L"Proporzioni", L"Seitenverhältnis", L"Relación de aspecto", L"Proporção de tela",
                                 L"Proporcje obrazu", L"Соотношение сторон", L"アスペクト比", L"화면비", L"宽高比", L"長寬比", L"อัตราส่วนภาพ"};
inline const Text AspectRatioHelp = {L"Shape of the picture. Black bars are added when it does not match the screen.",
                                     L"Forme de l'image. Des bandes noires sont ajoutées si elle ne correspond pas à l'écran.",
                                     L"Forma dell'immagine. Vengono aggiunte bande nere se non corrisponde allo schermo.",
                                     L"Form des Bildes. Passt sie nicht zum Bildschirm, werden schwarze Balken ergänzt.",
                                     L"Forma de la imagen. Se añaden bandas negras si no coincide con la pantalla.",
                                     L"Formato da imagem. Barras pretas são adicionadas quando não corresponde à tela.",
                                     L"Kształt obrazu. Jeśli nie pasuje do ekranu, dodawane są czarne pasy.",
                                     L"Форма изображения. Если она не совпадает с экраном, добавляются чёрные полосы.",
                                     L"映像の縦横比です。画面と合わない場合は黒帯が追加されます。",
                                     L"화면 비율입니다. 화면과 맞지 않으면 검은 띠가 추가됩니다.",
                                     L"画面比例。与屏幕不匹配时会添加黑边。",
                                     L"畫面比例。與螢幕不符時會加上黑邊。",
                                     L"รูปทรงของภาพ จะมีแถบสีดำเพิ่มเข้ามาเมื่อไม่ตรงกับหน้าจอ"};
inline const Text FillScreen = {L"Fill Screen", L"Plein écran", L"Riempi schermo", L"Bildschirm füllen", L"Llenar pantalla", L"Preencher tela", L"Wypełnij ekran",
                                L"Во весь экран", L"画面に合わせる", L"화면 채우기", L"填满屏幕", L"填滿螢幕", L"เต็มหน้าจอ"};

inline const Text HudLayout = {L"HUD Layout", L"Disposition de l'interface", L"Disposizione HUD", L"HUD-Anordnung", L"Disposición del HUD", L"Layout do HUD",
                               L"Układ HUD", L"Расположение интерфейса", L"HUDレイアウト", L"HUD 배치", L"HUD 布局", L"HUD 配置", L"การจัดวาง HUD"};
inline const Text HudLayoutHelp = {L"How the HUD fits a picture that is not 16:9: kept at 16:9, stretched over the whole screen, or with its elements moved to the screen edges.",
                                   L"Adaptation de l'interface à une image qui n'est pas en 16:9 : conservée en 16:9, étirée sur tout l'écran ou avec ses éléments déplacés vers les bords.",
                                   L"Come l'HUD si adatta a un'immagine non 16:9: mantenuto in 16:9, esteso su tutto lo schermo o con gli elementi spostati ai bordi.",
                                   L"Wie das HUD in ein Bild passt, das nicht 16:9 ist: bei 16:9 belassen, über den ganzen Bildschirm gestreckt oder mit den Elementen an den Bildschirmrändern.",
                                   L"Cómo se ajusta el HUD a una imagen que no es 16:9: se mantiene en 16:9, se estira por toda la pantalla o sus elementos se mueven a los bordes.",
                                   L"Como o HUD se ajusta a uma imagem que não é 16:9: mantido em 16:9, esticado pela tela inteira ou com os elementos movidos para as bordas.",
                                   L"Jak HUD dopasowuje się do obrazu innego niż 16:9: pozostaje w 16:9, jest rozciągnięty na cały ekran lub jego elementy trafiają do krawędzi ekranu.",
                                   L"Как интерфейс подстраивается под картинку не 16:9: остаётся 16:9, растягивается на весь экран или его элементы прижимаются к краям.",
                                   L"16:9以外の画面でのHUDの表示方法です。16:9のまま、画面全体に引き伸ばす、または要素を画面端に配置します。",
                                   L"16:9가 아닌 화면에서 HUD를 맞추는 방식입니다. 16:9 유지, 화면 전체로 늘리기, 또는 요소를 화면 가장자리로 이동합니다.",
                                   L"HUD 在非 16:9 画面中的显示方式：保持 16:9、拉伸至整个屏幕，或将元素移至屏幕边缘。",
                                   L"HUD 在非 16:9 畫面中的顯示方式：維持 16:9、拉伸至整個螢幕，或將元素移至螢幕邊緣。",
                                   L"วิธีจัด HUD เมื่อภาพไม่ใช่ 16:9: คงไว้ที่ 16:9 ยืดเต็มหน้าจอ หรือย้ายองค์ประกอบไปที่ขอบจอ"};
inline const Text Stretch = {L"Stretch", L"Étirer", L"Estendi", L"Strecken", L"Estirar", L"Esticar", L"Rozciągnij", L"Растянуть", L"引き伸ばす", L"늘리기", L"拉伸", L"拉伸", L"ยืด"};
inline const Text ScreenEdges = {L"Screen Edges", L"Bords de l'écran", L"Bordi dello schermo", L"Bildschirmränder", L"Bordes de la pantalla", L"Bordas da tela",
                                 L"Krawędzie ekranu", L"По краям экрана", L"画面端", L"화면 가장자리", L"屏幕边缘", L"螢幕邊緣", L"ขอบจอ"};

inline const Text Borderless = {L"Borderless Windowed", L"Fenêtré sans bordure", L"Finestra senza bordi", L"Randloses Fenster", L"Ventana sin bordes", L"Janela sem bordas",
                                L"Okno bez ramki", L"Окно без рамки", L"ボーダーレスウィンドウ", L"테두리 없는 창 모드", L"无边框窗口", L"無邊框視窗", L"หน้าต่างไร้ขอบ"};
} // namespace str
