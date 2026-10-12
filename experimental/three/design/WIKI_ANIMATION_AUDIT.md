# Wiki 动作名适配核对（2026-10-11）

来源：[Wiki Models](https://bluearchive.wiki/wiki/Models)，295 个模型；仓库版本 `525ae0faeb0a89f54ef4023f3be3692dcb529e51`。

仅用上次已读取的文件头 JSON 名称清单重测规则；本轮没有下载或导入新模型。名称核对不验证时长、挂件可见性、骨骼或视觉效果，仍由用户导入核对。

## 本轮适配结果

- 原有 46 个存在名称歧义的模型，目前仅剩 2 个无法单凭名字解决：Haruka 的重复 Exs、Yukari 的重复 Normal_Idle。
- 194 个可匹配至少一组战斗 Idle + Attack；278 个可匹配行走。没有战斗攻击的辅助/场景模型继续禁用战斗。
- 完整分组支持 Normal01/02、01_/02_、Normal_Attack01_Ing、末尾编号。默认选一套连贯组，优先普通人形、默认 / 01 组；不随机混合编号模式。
- 支持 Tank、Riding、ST / B_ST 家族；特殊家族的载具/挂件可见性仍须视觉核对。
- EX 多版本选择一段完整代表动作，排除 Idle / Spawn / Destroy、RootMotion、Cutin、召唤物及道具阶段，不自动拼接多段技能。
- 支持明确的拼写别名 Ilde、Callsigh、Strat、Destory。
- 重复同名动作：运行时只有在时长、通道、插值和关键帧数据完全相同才安全去重；不同数据继续跳过并写警告。名称预检没有动画数据，仍报告上述 2 个歧义。

已按 Wiki 名称测试 Eimi (Battle)、Hoshino (Battle / Swimsuit)、Kurumi、Toki、Shiroko＊Terror 的整套 01 组，并测试 Cherino (Hot Spring)、Iroha 的 Tank 家族、Yuzu (Maid) 的 ST 家族。无跨组拼接。

## 每模型摘要

| 模型 | 战斗姿态 | 选择的套组 | 行走动作 | 提示 |
| --- | --- | --- | --- | --- |
| Airi (Band).glb | Normal / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Airi.glb | — | Humanoid:default | Cafe_Walk | 战斗组缺项 |
| Akane (Bunny Girl).glb | — | Humanoid:default | Cafe_Walk | 战斗组缺项 |
| Akane (School Uniform).glb | — | Humanoid:default | Cafe_Walk | 战斗组缺项 |
| Akane.glb | Normal / Stand / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Akari (New Year).glb | Normal | Humanoid:default | Cafe_Walk | 无名称警告 |
| Akari.glb | Normal / Stand / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Ako (Dress).glb | Normal / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Ako.glb | — | Humanoid:default | Cafe_Walk | 战斗组缺项 |
| Aoba.glb | Normal | Humanoid:default | Cafe_Walk | 无名称警告 |
| Arisu (Battle) (Carrier).glb | — | Humanoid:default | Walk | 战斗组缺项 |
| Arisu (Battle) (Scenario).glb | Normal | Humanoid:default | Move_Ing | Idle 回退 |
| Arisu (Battle).glb | Normal | Humanoid:default | Cafe_Walk | 无名称警告 |
| Arisu (Maid).glb | Normal | Humanoid:default | Cafe_Walk | 无名称警告 |
| Arisu.glb | Normal | Humanoid:default | Cafe_Walk | 无名称警告 |
| Aru (Dress).glb | Normal / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Aru (New Year).glb | Normal / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Aru.glb | Normal / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Asuna (Bunny Girl).glb | Normal / Stand / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Asuna (School Uniform).glb | Normal / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Asuna.glb | Normal / Stand / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Atsuko (Swimsuit).glb | — | Humanoid:default | Cafe_Walk | 战斗组缺项 |
| Atsuko.glb | Normal / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Ayane (Swimsuit).glb | Normal | Humanoid:default | Cafe_Walk | 无名称警告 |
| Ayane.glb | — | Humanoid:default | Cafe_Walk | 战斗组缺项 |
| Azusa (Swimsuit).glb | Normal / Stand / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Azusa.glb | Normal / Stand / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Cherino (Hot Spring).glb | Normal | Tank:default | Cafe_Walk | 无名称警告 |
| Cherino.glb | Normal / Stand / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Chiaki.glb | Normal / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Chihiro.glb | — | Humanoid:default | Cafe_Walk | 战斗组缺项 |
| Chinatsu (Hot Spring).glb | Normal / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Chinatsu.glb | — | Humanoid:default | Cafe_Walk | 战斗组缺项 |
| Chise (Swimsuit).glb | Normal / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Chise.glb | Normal / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Eimi (Battle) (Carrier).glb | — | Humanoid:default | Cafe_Walk | 无名称警告 |
| Eimi (Battle) (Scenario).glb | — | Humanoid:default | — | Idle 回退；战斗组缺项 |
| Eimi (Battle).glb | Normal | Humanoid:01 | Cafe_Walk | 无名称警告 |
| Eimi (Swimsuit).glb | — | Humanoid:default | Cafe_Walk | 战斗组缺项 |
| Eimi.glb | Normal / Stand | Humanoid:default | Cafe_Walk | 无名称警告 |
| Eri.glb | Normal / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Erika.glb | — | Humanoid:default | Cafe_Walk | 战斗组缺项 |
| Fubuki (Swimsuit).glb | — | Humanoid:default | Cafe_Walk | 战斗组缺项 |
| Fubuki.glb | Normal / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Fuuka (New Year).glb | — | Humanoid:default | Cafe_Walk | 战斗组缺项 |
| Fuuka.glb | — | Humanoid:default | Cafe_Walk | 战斗组缺项 |
| Fuyu.glb | Normal / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Hanae (Christmas).glb | — | Humanoid:default | Cafe_Walk | 战斗组缺项 |
| Hanae.glb | — | Humanoid:default | Cafe_Walk | 战斗组缺项 |
| Hanako (Swimsuit).glb | Normal / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Hanako.glb | — | Humanoid:default | Cafe_Walk | 战斗组缺项 |
| Hare (Camping).glb | Normal / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Hare.glb | — | Humanoid:default | Cafe_Walk | 战斗组缺项 |
| Haruka (Dress).glb | Normal | Humanoid:default | Cafe_Walk | 无名称警告 |
| Haruka (New Year).glb | — | Humanoid:default | Cafe_Walk | 战斗组缺项 |
| Haruka.glb | Normal / Stand | Humanoid:default | Cafe_Walk | 同名歧义 |
| Haruna (New Year).glb | Normal / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Haruna (Sportswear).glb | — | Humanoid:default | Cafe_Walk | 战斗组缺项 |
| Haruna.glb | Normal / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Hasumi (Sportswear).glb | Normal / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Hasumi (Swimsuit).glb | Normal / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Hasumi.glb | Normal / Stand / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Hatsune Miku.glb | — | Humanoid:default | Cafe_Walk | 无名称警告 |
| Hibiki (Cheerleader).glb | Normal | Humanoid:default | Cafe_Walk | 无名称警告 |
| Hibiki.glb | — | Humanoid:default | Cafe_Walk | 战斗组缺项 |
| Hifumi (Swimsuit).glb | Normal | Tank:default | Cafe_Walk | 无名称警告 |
| Hifumi.glb | Normal / Stand / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Hikari.glb | Normal / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Himari (Battle).glb | Normal | Humanoid:default | Cafe_Walk | 无名称警告 |
| Himari.glb | — | Humanoid:default | Cafe_Walk | 战斗组缺项 |
| Hina (Dress).glb | Normal | Humanoid:default | Cafe_Walk | 无名称警告 |
| Hina (Swimsuit).glb | Normal | Humanoid:default | Cafe_Walk | 无名称警告 |
| Hina.glb | Normal / Stand | Humanoid:default | Cafe_Walk | 无名称警告 |
| Hinata (Swimsuit).glb | — | Humanoid:default | Cafe_Walk | 战斗组缺项 |
| Hinata.glb | Normal / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Hiyori (Swimsuit).glb | Normal / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Hiyori.glb | — | Humanoid:default | Cafe_Walk | 战斗组缺项 |
| Hoshino (Battle).glb | Normal | Humanoid:01 | 01_Cafe_Walk | 无名称警告 |
| Hoshino (Swimsuit).glb | Normal | Humanoid:01 | Cafe_Walk | 无名称警告 |
| Hoshino.glb | Normal / Stand | Humanoid:default | Cafe_Walk | 无名称警告 |
| Ibuki.glb | Normal / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Ichika (Swimsuit).glb | — | Humanoid:default | Cafe_Walk | 战斗组缺项 |
| Ichika.glb | Normal / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Iori (Swimsuit).glb | Normal / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Iori.glb | Normal / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Iroha.glb | Normal | Tank:default | Cafe_Walk | 无名称警告 |
| Izumi (New Year).glb | — | Humanoid:default | Cafe_Walk | 战斗组缺项 |
| Izumi (Swimsuit).glb | Normal | Humanoid:default | Cafe_Walk | 无名称警告 |
| Izumi.glb | Normal / Stand | Humanoid:default | Cafe_Walk | 无名称警告 |
| Izuna (Swimsuit).glb | Normal / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Izuna.glb | Normal / Stand / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Junko (New Year).glb | Normal / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Junko.glb | Normal / Stand / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Juri (Part-Timer).glb | Normal | Humanoid:default | Cafe_Walk | 无名称警告 |
| Juri.glb | — | Humanoid:default | Cafe_Walk | 战斗组缺项 |
| Kaede.glb | — | Humanoid:default | Cafe_Walk | 战斗组缺项 |
| Kaho.glb | Normal / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Kanna (Swimsuit).glb | Normal / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Kanna.glb | — | Humanoid:default | Cafe_Walk | 战斗组缺项 |
| Kanoe.glb | Normal | Humanoid:default | Cafe_Walk | 无名称警告 |
| Karin (Bunny Girl).glb | Normal / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Karin (School Uniform).glb | — | Humanoid:default | Cafe_Walk | 战斗组缺项 |
| Karin.glb | — | Humanoid:default | Cafe_Walk | 战斗组缺项 |
| Kasumi.glb | Normal / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Kayoko (Dress).glb | Normal / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Kayoko (New Year).glb | Normal / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Kayoko.glb | Normal / Stand / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Kazusa (Band).glb | Normal | Humanoid:default | Cafe_Walk | 无名称警告 |
| Kazusa.glb | Normal | Humanoid:default | Cafe_Walk | 无名称警告 |
| Kei (Carrier 2).glb | — | Humanoid:default | Walk | 战斗组缺项 |
| Kei (Carrier).glb | — | Humanoid:default | — | 战斗组缺项 |
| Kei (Prototype).glb | — | Humanoid:default | — | 无名称警告 |
| Kei (Scenario 1).glb | Normal | Humanoid:default | Move_Ing | Idle 回退 |
| Kei (Scenario 2).glb | Normal | Humanoid:default | Move_Ing | Idle 回退 |
| Kei.glb | Normal | Humanoid:default | Cafe_Walk | 无名称警告 |
| Kikyou (Swimsuit).glb | — | Humanoid:default | Cafe_Walk | 战斗组缺项 |
| Kikyou.glb | Normal / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Kirara.glb | Normal / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Kirino (Swimsuit).glb | — | Humanoid:default | Cafe_Walk | 战斗组缺项 |
| Kirino.glb | Normal / Stand / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Kisaki (Swimsuit).glb | Normal / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Kisaki.glb | — | Humanoid:default | Cafe_Walk | 战斗组缺项 |
| Koharu (Swimsuit).glb | Normal / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Koharu.glb | Normal / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Kokona.glb | Normal / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Konoka.glb | Normal | Humanoid:default | Cafe_Walk | 无名称警告 |
| Kotama (Camping).glb | Normal / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Kotama.glb | Normal / Stand / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Kotori (Cheerleader).glb | Normal | Humanoid:default | Cafe_Walk | 无名称警告 |
| Kotori.glb | Normal / Stand | Humanoid:default | Cafe_Walk | 无名称警告 |
| Koyuki (Pajama).glb | — | Humanoid:default | Cafe_Walk | 战斗组缺项 |
| Koyuki.glb | Normal | Humanoid:default | Cafe_Walk | 无名称警告 |
| Kurumi.glb | Normal | Humanoid:01 | Cafe_Walk | 无名称警告 |
| Maki (Camping).glb | — | Humanoid:default | Cafe_Walk | 战斗组缺项 |
| Maki.glb | Normal | Humanoid:default | Cafe_Walk | 无名称警告 |
| Makoto.glb | — | Humanoid:default | Cafe_Walk | 战斗组缺项 |
| Mari (Idol).glb | Normal / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Mari (Sportswear).glb | Normal / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Mari.glb | — | Humanoid:default | Cafe_Walk | 战斗组缺项 |
| Marina (Qipao).glb | Normal / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Marina.glb | Normal / Stand / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Mashiro (Swimsuit).glb | — | Humanoid:default | Cafe_Walk | 战斗组缺项 |
| Mashiro.glb | — | Humanoid:default | Cafe_Walk | 战斗组缺项 |
| Material Development Club Member.glb | — | Humanoid:default | — | Idle 回退 |
| Megu.glb | Normal | Humanoid:default | Cafe_Walk | 无名称警告 |
| Meru.glb | Normal / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Michiru (Dress).glb | — | Humanoid:default | Cafe_Walk | 战斗组缺项 |
| Michiru.glb | Normal | Humanoid:default | Cafe_Walk | 无名称警告 |
| Midori (Battle).glb | — | Humanoid:default | Walk | 战斗组缺项 |
| Midori (Maid).glb | Normal / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Midori.glb | Normal / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Mika (Cut-in).glb | — | Humanoid:default | — | Idle 回退 |
| Mika (Swimsuit).glb | Normal / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Mika.glb | Normal / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Mimori (Swimsuit).glb | — | Humanoid:default | Cafe_Walk | 战斗组缺项 |
| Mimori.glb | Normal / Stand / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Mina.glb | Normal / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Mine (Idol).glb | Normal | Humanoid:default | Cafe_Walk | 无名称警告 |
| Mine.glb | Normal | Humanoid:default | Cafe_Walk | 无名称警告 |
| Minori.glb | — | Humanoid:default | Cafe_Walk | 战斗组缺项 |
| Misaka Mikoto.glb | Normal / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Misaki (Swimsuit).glb | — | Humanoid:default | Cafe_Walk | 战斗组缺项 |
| Misaki.glb | Normal / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Miyako (Swimsuit).glb | Normal / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Miyako.glb | Normal / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Miyo.glb | Normal / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Miyu (Swimsuit).glb | — | Humanoid:default | Cafe_Walk | 战斗组缺项 |
| Miyu.glb | Normal / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Moe (Swimsuit).glb | Normal / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Moe.glb | — | Humanoid:default | Cafe_Walk | 战斗组缺项 |
| Momiji.glb | Normal / Stand / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Momoi (Battle).glb | — | Humanoid:default | Walk | 战斗组缺项 |
| Momoi (Maid).glb | Normal / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Momoi.glb | Normal / Stand / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Mutsuki (Dress).glb | — | Humanoid:default | Cafe_Walk | 战斗组缺项 |
| Mutsuki (New Year).glb | Normal | Humanoid:default | Cafe_Walk | 无名称警告 |
| Mutsuki.glb | Normal / Stand | Humanoid:default | Cafe_Walk | 无名称警告 |
| Nagisa (Swimsuit).glb | — | Humanoid:default | Cafe_Walk | 战斗组缺项 |
| Nagisa.glb | — | Humanoid:default | Cafe_Walk | 战斗组缺项 |
| Nagusa (Cut-in).glb | — | Humanoid:default | — | 无名称警告 |
| Nagusa (Swimsuit).glb | Normal | Humanoid:default | Cafe_Walk | 无名称警告 |
| Nagusa.glb | Normal | Humanoid:default | Cafe_Walk | 无名称警告 |
| Natsu (Band) (No Weapon).glb | — | Humanoid:default | — | 战斗组缺项 |
| Natsu (Band).glb | Normal / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Natsu.glb | Normal | Humanoid:default | Cafe_Walk | 无名称警告 |
| Neru (Bunny Girl).glb | Normal / Stand / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Neru (School Uniform).glb | Normal / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Neru.glb | Normal / Stand / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Niko.glb | Normal | Humanoid:default | Cafe_Walk | 无名称警告 |
| Niya.glb | — | Humanoid:default | Cafe_Walk | 战斗组缺项 |
| Noa (Pajama).glb | Normal / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Noa.glb | Normal / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Nodoka (Hot Spring).glb | — | Humanoid:default | Cafe_Walk | 战斗组缺项 |
| Nodoka.glb | — | Humanoid:default | Cafe_Walk | 战斗组缺项 |
| Nonomi (Swimsuit).glb | Normal | Humanoid:default | Cafe_Walk | 无名称警告 |
| Nonomi.glb | Normal / Stand | Humanoid:default | Move_Ing | Idle 回退 |
| Nozomi.glb | Normal / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Otogi.glb | Normal / Kneel | Humanoid:01 | Cafe_Walk | 无名称警告 |
| Pina (Guide).glb | — | Humanoid:default | Cafe_Walk | 战斗组缺项 |
| Pina.glb | Normal | Humanoid:default | Cafe_Walk | 无名称警告 |
| Rabu.glb | Normal | Humanoid:default | Cafe_Walk | 无名称警告 |
| Rei.glb | Normal / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Reijo.glb | Normal / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Reisa (Magical).glb | — | Humanoid:default | Cafe_Walk | 战斗组缺项 |
| Reisa.glb | Normal | Humanoid:default | Cafe_Walk | 无名称警告 |
| Rena.glb | Normal / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Renge (Swimsuit).glb | Normal / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Renge.glb | Normal / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Rio (Battle) (Scenario).glb | — | Humanoid:default | — | Idle 回退 |
| Rio (Battle).glb | Normal / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Rio.glb | — | Humanoid:default | Cafe_Walk | 战斗组缺项 |
| Ritsu.glb | — | Humanoid:default | Cafe_Walk | 战斗组缺项 |
| Rumi.glb | Normal / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Saki (Swimsuit).glb | Normal | Humanoid:default | Cafe_Walk | 无名称警告 |
| Saki.glb | — | Humanoid:default | Cafe_Walk | 战斗组缺项 |
| Sakurako (Idol).glb | Normal / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Sakurako.glb | Normal / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Saori (Dress) (Cafe).glb | — | Humanoid:default | Cafe_Walk | 无名称警告 |
| Saori (Dress).glb | Normal | Humanoid:default | Move_Ing | 无名称警告 |
| Saori (Swimsuit).glb | Normal / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Saori.glb | Normal / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Saten Ruiko.glb | — | Humanoid:default | Cafe_Walk | 无名称警告 |
| Satsuki.glb | — | Humanoid:default | Cafe_Walk | 战斗组缺项 |
| Saya (Casual).glb | — | Humanoid:default | Cafe_Walk | 战斗组缺项 |
| Saya.glb | — | Humanoid:default | Cafe_Walk | 战斗组缺项 |
| Seia (Swimsuit).glb | Normal / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Seia.glb | Normal / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Sena (Casual).glb | Normal / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Sena.glb | Normal | Humanoid:default | Cafe_Walk | 无名称警告 |
| Serika (New Year).glb | — | Humanoid:default | Cafe_Walk | 战斗组缺项 |
| Serika (Swimsuit).glb | — | Humanoid:default | Cafe_Walk | 战斗组缺项 |
| Serika.glb | Normal / Stand / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Serina (Christmas).glb | Normal / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Serina.glb | — | Humanoid:default | Cafe_Walk | 战斗组缺项 |
| Shigure (Hot Spring).glb | — | Humanoid:default | Cafe_Walk | 战斗组缺项 |
| Shigure.glb | Normal / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Shimiko.glb | Normal / Stand / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Shiroko (Riding).glb | Normal / Stand / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Shiroko (Swimsuit).glb | — | Humanoid:default | Cafe_Walk | 战斗组缺项 |
| Shiroko.glb | Normal / Stand / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Shiroko＊Terror.glb | Normal | Humanoid:01 | Cafe_Walk | 无名称警告 |
| Shizuko (Swimsuit).glb | — | Humanoid:default | Cafe_Walk | 战斗组缺项 |
| Shizuko.glb | — | Humanoid:default | Cafe_Walk | 战斗组缺项 |
| Shokuhou Misaki.glb | Normal / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Shun (Kid).glb | Normal / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Shun (Swimsuit) (1).glb | Normal / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Shun (Swimsuit) (2).glb | Normal / Kneel | Humanoid:default | Move_Ing | 无名称警告 |
| Shun.glb | Normal / Kneel | Humanoid:01 | Cafe_Walk | 无名称警告 |
| Subaru.glb | Normal / Kneel | Humanoid:01 | Cafe_Walk | 无名称警告 |
| Sumire (Part-Timer).glb | — | Humanoid:default | Cafe_Walk | 战斗组缺项 |
| Sumire.glb | Normal | Humanoid:default | Cafe_Walk | 无名称警告 |
| Suzumi (Magical).glb | Normal / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Suzumi.glb | Normal / Stand / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Takane.glb | Normal / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Toki (Battle) (Scenario).glb | Normal | Humanoid:default | Move_Ing | Idle 回退 |
| Toki (Battle).glb | Normal / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Toki (Bunny Girl).glb | Normal / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Toki (Cafe).glb | — | Humanoid:default | Cafe_Walk | 无名称警告 |
| Toki.glb | Normal | Humanoid:01 | Move_Ing01 | 无名称警告 |
| Tomoe (Qipao).glb | — | Humanoid:default | Cafe_Walk | 战斗组缺项 |
| Tomoe.glb | Normal / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Tsubaki (Guide).glb | — | Humanoid:default | Cafe_Walk | 战斗组缺项 |
| Tsubaki.glb | Normal | Humanoid:default | Cafe_Walk | 无名称警告 |
| Tsukuyo (Dress).glb | Normal | Humanoid:default | Cafe_Walk | 无名称警告 |
| Tsukuyo.glb | Normal / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Tsurugi (Swimsuit).glb | Normal | Humanoid:01 | Cafe_Walk | 无名称警告 |
| Tsurugi.glb | Normal | Humanoid:01 | Cafe_Walk | 无名称警告 |
| Ui (Swimsuit).glb | Normal / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Ui.glb | Normal / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Umika.glb | Normal / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Utaha (Cheerleader).glb | Normal | Humanoid:default | Cafe_Walk | 无名称警告 |
| Utaha.glb | — | Humanoid:default | Cafe_Walk | 战斗组缺项 |
| Wakamo (Swimsuit).glb | Normal / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Wakamo.glb | Normal / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Yakumo.glb | — | Humanoid:default | Cafe_Walk | 战斗组缺项 |
| Yoshimi (Band).glb | Normal / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Yoshimi.glb | — | Humanoid:default | Cafe_Walk | 战斗组缺项 |
| Yukari (Swimsuit).glb | Normal / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Yukari.glb | — | Humanoid:default | Cafe_Walk | 同名歧义；战斗组缺项 |
| Yuuka (Pajama).glb | Normal / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Yuuka (Sportswear).glb | Normal / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Yuuka.glb | Normal / Stand / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| Yuzu (Battle) (Carrier).glb | — | Humanoid:default | Cafe_Walk | 无名称警告 |
| Yuzu (Battle) (Cut-in).glb | — | Humanoid:default | — | 无名称警告 |
| Yuzu (Battle).glb | — | Humanoid:default | Cafe_Walk | 战斗组缺项 |
| Yuzu (Maid).glb | Normal | ST:default | Cafe_Walk | 无名称警告 |
| Yuzu.glb | Normal / Kneel | Humanoid:default | Cafe_Walk | 无名称警告 |
| ladies_biker_ar.glb | — | Humanoid:default | — | 无名称警告 |
| ladies_biker_mg.glb | — | Humanoid:default | — | 无名称警告 |
| ladies_biker_sg.glb | — | Humanoid:default | — | 无名称警告 |
| ladiesboss_biker_ar.glb | — | Humanoid:default | — | 无名称警告 |
| sm030001.glb | — | Humanoid:default | — | Idle 回退；战斗组缺项 |
| sm032601.glb | — | Humanoid:default | — | Idle 回退；战斗组缺项 |
| st0004.glb | — | Humanoid:default | — | 无名称警告 |
| st0005.glb | — | Humanoid:default | — | 无名称警告 |

完整原名、选择组、备选组和缺项见 `artifacts/three-wiki-name-compatibility.json`。
