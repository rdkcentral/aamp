# Introduction

Metrics Library is an independent library for session statistics generation/reporting in JSON format. Metrics will save download information, bitrate switching and license statistics, depending on each downloads.

Here is a brief introduction to the workflow of Metrics library:

(1) Metrics library requires the bitrate, download time, track/stream type to save fragment/manifest/init fragment statistics.

(2) Metrics library saves available audio languages based on audio profile index

(3) Metrics also keep record of license encryption statistics and ABR drop down counts.

``
