# obs-streamelements-installer

obs-streamelements with optional obs-studio component installation package for Windows.

# Filesystem

+ root
  |
  +-- obs-streamelements-installer         /* THIS FOLDER */
  +-- obs-studio                           /* obs-studio source folder */
      |
      +-- rundir/RelWithDebInfo            /* obs-studio build output folder */

# Building

1. Build obs-studio RelWidhDebInfo configuration under "obs-studio" folder located
   in the parent folder (see Filesystem section)

2. Download & install NSIS: http://nsis.sourceforge.net/Download

3. Download NSIS Inetc plug-in and extract the downloaded ZIP contenst in <nsis-install-location>

4. Open command prompt & execute: <nsis-install-location>\makensis.exe main.nsi

# Release procedure

* Build obs-streamelements-setup.exe (obs-streamelements-installer)
* Sign obs-streamelements-setup.exe with the code signing EV token on a sterile machine
* Make sure that the signed EXE is clean according to the procedure below
* Submit the EXE for QA
* Once approved:
      - Back-up production version of the EXE and manifest
      - Overwrite the production version of the EXE and manifest with files from QA

# Pre-release requirements

Once obs-streamelements-setup.exe is built and signed, check that it's completely clean with the following online scanners:

* https://virustotal.com
* https://www.hybrid-analysis.com/

In case one of the engines at virustotal.com displays a false positive, report it to the relevant engine contact below:

360:
kefu@360.cn

AegisLab
support@aegislab.com

Agnitum:
trojans@agnitum.com

Ahnlab:
e-support@ahnlab.com, samples@ahnlab.com

Alibaba:
virustotal@list.alibaba-inc.com

Alyac (Estsoft):
esrc@estsecurity.com

Antivir
cleanset@avira.com, virus_malware@avira.com, virus@avira.com

Antiy
avlsdk_support_vt@antiy.cn

Avast:
virus@avast.com

AVG
http://www.avg.com/submit-sample
http://www.avg.com/us-en/whitelist

Baidu
bav@baidu.com, gaoyingchun@baidu.com

BitDefender
virus_submission@bitdefender.com

Bkav
fpreport@bkav.com, bkav@bkav.com

ByteHero
bytehero@163.com

ClamAV
http://www.clamav.net/reports/fp

CMC
vulambang@cmcinfosec.com, support.is@cmclab.net

Comodo:
malwaresubmit@avlab.comodo.com

CrowdStrike
VTscanner@crowdstrike.com

Cybereason
vt-feedback@cybereason.com

Cylance:
cylancefilesubmit@cylance.com

CyRadar
virustotal@cyradar.com

DNS8
dns8@layer8.pt

DrWeb:
vms@drweb.com

eGambit (Tehtris)
https://tehtris.com/egambit_fp.php
virus@tehtri-security.com

Emsisoft:
submit@emsisoft.com or fp@emsisoft.com (false positives)
https://www.emsisoft.com/en/support/contact/

Endgame:
info@endgame.com

ESET:
https://support.eset.com/kb141/?page=content&id=SOLN141

F-Prot:
viruslab@f-prot.com

F-Secure:
spyware-samples@f-secure.com, vsamples@f-secure.com

Forcepoint (websense)
suggest@forcepoint.com

Fortinet
submitvirus@fortinet.com

GData
https://www.gdatasoftware.com/faq/consumer/submit-a-suspicious-file-app-or-url

Hacksoft:
virus@hacksoft.com.pe

Hauri:
viruslab@hauri.co.kr

Ikarus
fp@ikarus.at, samples@ikarus.at

Invincea:
info@invincea.com

Jiangmin
support@jiangmin.com, shaojia@jiangmin.com

K7
reportfp@labs.k7computing.com, k7viruslab@labs.k7computing.com

Kaspersky:
newvirus@kaspersky.com

Kingsoft (Cheetah):
operation@cmcm.com

MAX (SaintSecurity):
root@malwares.com

McAfee
virus_research@mcafee.com

McAfee-GW
virus_research_gateway@avertlabs.com

Microsoft
mmpc@submit.microsoft.com

Microworld:
samples@escanav.com

NANO
http://www.nanoav.ru/index.php?option=com_content&view=article&id=15&Itemid=83&lang=en
false@nanoav.ru

Norman:
analysis@norman.no, support@norman.com

nProtect (Inca):
virus_info@inca.co.kr

Palo Alto
vt-pan-false-positive@paloaltonetworks.com

Panda
falsepositives@pandasecurity.com, virussamples@pandasecurity.com

Rising
http://mailcenter.rising.com.cn/filecheck_en/

QuickHeal:
viruslab@quickheal.com

Sentinel One:
report@sentinelone.com

Sophos:
samples@sophos.com

Symantec
https://submit.symantec.com/false_positive/, avsubmit@symantec.com

Tencent:
TAVfp@tencent.com

TheHacker:
virus@hacksoft.com.pe, falsopositivo@hacksoft.com.pe

TrendMicro
https://www.trendmicro.com/en_us/about/legal/detection-reevaluation.html
virus@trendmicro.com, virus_doctor@trendmicro.com
http://esupport.trendmicro.com/solution/en-us/1037634.aspx

Webroot
https://www.webroot.com/us/en/business/support/vendor-dispute-contact-us

Trustwave
ADavidi@trustwave.com

VBA32:
feedback@anti-virus.by

VirusDie
partners@virusdie.com

WhiteArmor:
obu@whitearmor.ai

Yandex:
yandex-antivir@support.yandex.ru

Zillya:
virus@zillya.com

Zoner:
false@zonerantivirus.com
