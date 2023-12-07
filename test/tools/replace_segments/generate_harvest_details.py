from pathlib import Path
import re
import os
import argparse
from library.dash_manifests import DASHManifest
import shutil
import json
from datetime import datetime
from urllib.parse import urlparse

def get_mpd_list(search_path = os.getcwd()):
    """
    Searches for mpd files by scanning given path

    :param search_path: root path of harvested dump 
    :return: list of manifest files available 
    """
    all_mpds = []
    mpd_base_dir = []
    try:
        for path in Path(search_path).rglob('*.mpd*'):
            mpd = str(path.relative_to(search_path))
            all_mpds.append(mpd)
            mpd_base_dir.append(os.path.dirname(mpd))
        pattern = ".orig[0-9_]+"
        all_mpds = sorted(all_mpds,key=lambda x: not re.search(pattern, x))
        all_mpds.sort(key=lambda f: int(''.join(filter(str.isdigit, f))))
        mpd_base_dir = max(set(mpd_base_dir), key=mpd_base_dir.count)
        all_mpds = [m for m in all_mpds if mpd_base_dir in m and not re.search(pattern, m)]
    except Exception as e:
        print(f"Error -> {e}")
    return all_mpds

def check_header_file(search_path = os.getcwd(), representation_id=""):
    """
    Verifies header file availability as per representations in manifest

    :param search_path: root path of harvested dump 
    :param representation_id: representation_id available in manifest 
    :return: boolean True if file is available. 
    """
    try:
        for path in Path(search_path).rglob(f"*{representation_id}*"):
            return True
    except Exception as e:
        print(f"Error -> {e}")
        return False

def remove_non_harvested_representations(period, available_segment_paths):
    """
    Removes representations (from manifest files) which are not harvested

    :param period: period tag from manifest 
    """
    if period.tag.endswith("Period"):
        period_id = period.attrib.get('id', "")
        segment_dir_path = ""
        for templ in period:
            # replace this BaseURL after transcoding is complete
            if templ.tag.endswith("BaseURL"): 
                BaseURL_properties = urlparse(templ.text)
                segment_dir_path = f"{BaseURL_properties.netloc}{BaseURL_properties.path}"
        for period_child in period:
            if period_child.tag.endswith('AdaptationSet'):
                adpset = period_child
                repr_set = []
                initialization = ""
                for adpset_child in adpset:
                    if "AD" in period_id:                    
                        if adpset_child.tag.endswith('Representation'):
                            repr = adpset_child
                            repr_id = repr.attrib.get('id')
                            seg_tmpl_set = []
                            for repr_child in repr:
                                if repr_child.tag.endswith('SegmentTemplate'):
                                    seg_template = repr_child
                                    initialization_str = seg_template.attrib.get('initialization')
                                    if check_header_file(search_path=segment_dir_path, representation_id = initialization):
                                        seg_tmpl_set.append(initialization_str)
                                    else:
                                        seg_template.tag = re.sub('SegmentTemplate','SegmentTemplate_BYPASSED', seg_template.tag)
                            if len(seg_tmpl_set) == 0:
                                repr.tag = re.sub('Representation','Representation_BYPASSED',repr.tag)
                            else:
                                repr_set.append(repr_id)
                    else:
                        if adpset_child.tag.endswith('SegmentTemplate'):
                            seg_template = adpset_child
                            init = seg_template.attrib.get('initialization',"")
                            if "$" not in init:
                                initialization = init

                for adpset_child in adpset:
                    if "AD" not in period_id:
                        if initialization != "":
                            for ap in available_segment_paths:
                                if initialization in ap:
                                    repr_set.append(initialization)
                                    break
                            else:
                               break 
                        elif adpset_child.tag.endswith('Representation'):
                            repr = adpset_child
                            repr_id = repr.attrib.get('id')
                            if check_header_file(search_path=segment_dir_path, representation_id = repr_id):
                                repr_set.append(repr_id)
                            else:
                                repr.tag = re.sub('Representation','Representation_BYPASSED', repr.tag)
                if len(repr_set) == 0:
                    adpset.tag = re.sub('AdaptationSet','AdaptationSet_BYPASSED',adpset.tag)


def get_segments_path(dash):
    available_segments = []
    for seg_list in dash.get_seg_list(abs_paths=True):
        if os.path.isfile(seg_list["segment_name"]):
            available_segments.append(seg_list["segment_name"])
    # print(f"{available_segments = }")
    return available_segments


def generate_harvest_details(root_dir = os.getcwd()):
    """
    Checks harvest_details.json available or not and Generates if file is not available. 

    :param root_dir: root path of harvested dump 
    :return: boolean True if file is available. Otherwise harvest_details.json is generated in root_dir.
    """
    harvest_details_path = f"{root_dir}/harvest_details.json"

    if os.path.isfile(harvest_details_path):
        d = {}
        with open(harvest_details_path) as f:
            d = json.load(f)
            if "url" in d:
                return True # harvest_details.json is already present
    mpd_list = get_mpd_list()

    segment_directories = []
    for mpd in mpd_list:
        original_file = f"{mpd}.orig{datetime.utcnow():%y%m%d_%H%M%S}"
        
        dash = DASHManifest(mpd)
        shutil.copy(mpd,original_file)
        
        for period in dash.root:
            if period.tag.endswith("Period"):
                for templ in period:
                    if templ.tag.endswith("BaseURL"):
                        url_properties = urlparse(templ.text)
                        segment_directories.append(f"{url_properties.netloc}{url_properties.path}")
                available_segment_paths = get_segments_path(dash)
                
                remove_non_harvested_representations(period, available_segment_paths)
        
        with open(mpd, "w") as f_stream:
            f_stream.write(str(dash))
    original_mpd_name = f'{mpd_list[0].split(".mpd.",1)[0]}.mpd'
    # unique_segment_directories = max(set(segment_directories), key=segment_directories.count)
    unique_segment_directories = {i:segment_directories.count(i) for i in set(segment_directories)}
    contains_ad_segments = False
    if len(unique_segment_directories) > 1:
        contains_ad_segments = True

    harvest_details = {
        "recording_start_time": datetime.utcnow().strftime('%Y-%m-%dT%H:%M:%S.%f'),
        "args": [
            "../harvest.py",
            "-b",
            "562800",
            "-m",
            "1800",
            f"/{original_mpd_name}", # "https://lin012-gb-s8-prd-ak.cdn01.skycdp.com/v1/frag/bmff/enc/cenc/t/SKYATHD_HD_SU_SKYUK_4053_0_6139857640084951163.mpd"
        ],
        "url": f"/{original_mpd_name}", #"https://lin012-gb-s8-prd-ak.cdn01.skycdp.com/v1/frag/bmff/enc/cenc/t/SKYATHD_HD_SU_SKYUK_4053_0_6139857640084951163.mpd"
        "contains_ad_segments": contains_ad_segments,
    }
    print(harvest_details)
    json.dump(harvest_details, open(harvest_details_path, "w"))

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Harvests content from media streams")
    parser.add_argument(
        "-r",
        "--root",
        help="""Location to store the harvested content. Defaults to current working directory
                        """,
        default=os.getcwd()
    )

    args = parser.parse_args()
    if args.root is not None:
        os.chdir(args.root)

    generate_harvest_details(args.root)
    
